/*
 * Copyright 2025-2026 AxionOS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AxRamPlus"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>

namespace {

constexpr char kDataPath[] = "/data";
constexpr char kRamPlusPath[] = "/data/system/axion_ram_plus";
constexpr char kSwapPath[] = "/data/system/axion_ram_plus/swapfile";
constexpr char kMkswapPath[] = "/system/bin/mkswap";
constexpr char kRequestedSizeProp[] = "persist.sys.ax_rp_size_mb";
constexpr char kSupportedProp[] = "persist.sys.ax_rp_supp";
constexpr char kActiveSizeProp[] = "persist.sys.ax_rp_active_kb";
constexpr char kStatusProp[] = "persist.sys.ax_rp_status";
constexpr uint64_t kBytesPerMib = 1024ULL * 1024ULL;
constexpr uint64_t kKilobytesPerMib = 1024ULL;
constexpr uint64_t kSwapSizeToleranceKb = 1024ULL;
constexpr uint64_t kStatBlockSize = 512ULL;
constexpr uint64_t kWriteChunkBytes = 4ULL * kBytesPerMib;
constexpr uint32_t kDefaultReserveMib = 4096;
constexpr auto kZramPollInterval = std::chrono::milliseconds(250);
constexpr int kZramPollAttempts = 60;

struct SwapEntry {
    std::string path;
    uint64_t size_kb;
    int priority;
};

enum class PrepareResult {
    kOk,
    kInsufficientSpace,
    kFileError,
    kAllocationError,
};

bool IsAllowedSize(uint32_t size_mib) {
    return size_mib == 0 || size_mib == 2048 || size_mib == 4096 || size_mib == 6144 ||
           size_mib == 8192;
}

bool PublishStatus(uint64_t active_kb, const std::string& status) {
    const bool size_set = android::base::SetProperty(kActiveSizeProp, std::to_string(active_kb));
    const bool status_set = android::base::SetProperty(kStatusProp, status);
    if (!size_set || !status_set) {
        LOG(ERROR) << "Failed to publish RAM Plus status";
    }
    return size_set && status_set;
}

bool ReadSwapEntries(std::vector<SwapEntry>* entries) {
    std::string contents;
    if (!android::base::ReadFileToString("/proc/swaps", &contents)) {
        return false;
    }

    std::istringstream stream(contents);
    std::string line;
    std::getline(stream, line);
    entries->clear();

    std::string path;
    std::string type;
    uint64_t size_kb;
    uint64_t used_kb;
    int priority;
    while (stream >> path >> type >> size_kb >> used_kb >> priority) {
        entries->push_back({path, size_kb, priority});
    }
    return true;
}

std::optional<int> FindLowestZramPriority(const std::vector<SwapEntry>& entries) {
    std::optional<int> priority;
    for (const auto& entry : entries) {
        if (entry.path.rfind("/dev/block/zram", 0) != 0 && entry.path.rfind("/dev/zram", 0) != 0) {
            continue;
        }
        priority = priority.has_value() ? std::min(*priority, entry.priority) : entry.priority;
    }
    return priority;
}

std::optional<int> WaitForZramPriority() {
    for (int attempt = 0; attempt < kZramPollAttempts; ++attempt) {
        std::vector<SwapEntry> entries;
        if (ReadSwapEntries(&entries)) {
            if (const auto priority = FindLowestZramPriority(entries); priority.has_value()) {
                return priority;
            }
        }
        std::this_thread::sleep_for(kZramPollInterval);
    }
    return std::nullopt;
}

bool EnsureDataDirectory() {
    struct stat st = {};
    if (lstat(kRamPlusPath, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            LOG(ERROR) << kRamPlusPath << " is not a directory";
            return false;
        }
    } else if (errno == ENOENT) {
        if (mkdir(kRamPlusPath, 0700) != 0) {
            PLOG(ERROR) << "Failed to create " << kRamPlusPath;
            return false;
        }
    } else {
        PLOG(ERROR) << "Failed to inspect " << kRamPlusPath;
        return false;
    }

    if (chmod(kRamPlusPath, 0700) != 0) {
        PLOG(ERROR) << "Failed to secure " << kRamPlusPath;
        return false;
    }
    return true;
}

std::optional<bool> HasAllocationSpace(uint64_t current_bytes, uint64_t requested_bytes) {
    if (requested_bytes <= current_bytes) {
        return true;
    }

    struct statvfs fs = {};
    if (statvfs(kDataPath, &fs) != 0) {
        PLOG(ERROR) << "Failed to read /data storage information";
        return std::nullopt;
    }

    const uint64_t available_bytes = static_cast<uint64_t>(fs.f_bavail) * fs.f_frsize;
    const uint64_t required_bytes =
            requested_bytes - current_bytes + kDefaultReserveMib * kBytesPerMib;
    return available_bytes >= required_bytes;
}

bool WriteZeroes(android::base::borrowed_fd fd, uint64_t start, uint64_t end) {
    std::vector<uint8_t> zeroes(kWriteChunkBytes);
    for (uint64_t offset = start; offset < end;) {
        const size_t size = std::min<uint64_t>(zeroes.size(), end - offset);
        if (!android::base::WriteFullyAtOffset(fd, zeroes.data(), size, offset)) {
            PLOG(ERROR) << "Failed to allocate RAM Plus swapfile";
            return false;
        }
        offset += size;
    }
    return true;
}

PrepareResult PrepareSwapFile(uint64_t requested_bytes) {
    android::base::unique_fd fd(
            TEMP_FAILURE_RETRY(open(kSwapPath, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600)));
    if (!fd.ok()) {
        PLOG(ERROR) << "Failed to open " << kSwapPath;
        return PrepareResult::kFileError;
    }

    struct stat st = {};
    if (fstat(fd.get(), &st) != 0) {
        PLOG(ERROR) << "Failed to inspect RAM Plus swapfile";
        return PrepareResult::kFileError;
    }
    if (!S_ISREG(st.st_mode)) {
        LOG(ERROR) << "RAM Plus swapfile is not a regular file";
        return PrepareResult::kFileError;
    }
    if (fchmod(fd.get(), 0600) != 0) {
        PLOG(ERROR) << "Failed to secure RAM Plus swapfile";
        return PrepareResult::kFileError;
    }
    const uint64_t allocated_bytes =
            st.st_blocks > 0 ? static_cast<uint64_t>(st.st_blocks) * kStatBlockSize : 0;
    const uint64_t current_bytes = st.st_size > 0 ? static_cast<uint64_t>(st.st_size) : 0;
    const auto has_space =
            HasAllocationSpace(std::min(allocated_bytes, requested_bytes), requested_bytes);
    if (!has_space.has_value()) {
        return PrepareResult::kFileError;
    }
    if (!*has_space) {
        return PrepareResult::kInsufficientSpace;
    }
    if (ftruncate(fd.get(), requested_bytes) != 0) {
        PLOG(ERROR) << "Failed to resize RAM Plus swapfile";
        return PrepareResult::kAllocationError;
    }
    const uint64_t preserved_bytes = std::min(current_bytes, requested_bytes);
    const uint64_t write_offset = allocated_bytes < preserved_bytes ? 0 : preserved_bytes;
    if (!WriteZeroes(fd, write_offset, requested_bytes)) {
        return PrepareResult::kAllocationError;
    }
    if (fsync(fd.get()) != 0) {
        PLOG(ERROR) << "Failed to sync RAM Plus swapfile";
        return PrepareResult::kFileError;
    }
    return PrepareResult::kOk;
}

bool RunMkswap() {
    const pid_t pid = fork();
    if (pid < 0) {
        PLOG(ERROR) << "Failed to fork mkswap";
        return false;
    }
    if (pid == 0) {
        execl(kMkswapPath, kMkswapPath, kSwapPath, static_cast<char*>(nullptr));
        _exit(127);
    }

    int status;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        PLOG(ERROR) << "Failed to wait for mkswap";
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::optional<SwapEntry> FindRamPlusSwap() {
    std::vector<SwapEntry> entries;
    if (!ReadSwapEntries(&entries)) {
        return std::nullopt;
    }
    const auto entry = std::find_if(entries.begin(), entries.end(), [](const auto& candidate) {
        return candidate.path == kSwapPath;
    });
    return entry == entries.end() ? std::nullopt : std::optional<SwapEntry>(*entry);
}

bool DisableSwap() {
    if (swapoff(kSwapPath) == 0 || errno == EINVAL || errno == ENOENT) {
        return true;
    }
    PLOG(ERROR) << "Failed to disable RAM Plus swap";
    return false;
}

bool RemoveSwapFile() {
    if (unlink(kSwapPath) == 0 || errno == ENOENT) {
        return true;
    }
    PLOG(ERROR) << "Failed to remove RAM Plus swapfile";
    return false;
}

int Fail(const std::string& status) {
    PublishStatus(0, status);
    return 1;
}

int FailPreparedSwap(const std::string& status) {
    return RemoveSwapFile() ? Fail(status) : Fail("file_error");
}

int FailActiveSwap(uint64_t active_kb, const std::string& status) {
    if (!DisableSwap()) {
        PublishStatus(active_kb, "swapoff_failed");
        return 1;
    }
    return FailPreparedSwap(status);
}

}

int main() {
    if (!android::base::GetBoolProperty(kSupportedProp, false)) {
        return Fail("unsupported");
    }

    uint32_t requested_mib;
    if (!android::base::ParseUint(android::base::GetProperty(kRequestedSizeProp, ""),
                                  &requested_mib) ||
        !IsAllowedSize(requested_mib)) {
        return Fail("invalid_size");
    }
    if (requested_mib == 0) {
        if (!DisableSwap()) {
            return Fail("swapoff_failed");
        }
        if (!RemoveSwapFile()) {
            return Fail("file_error");
        }
        return PublishStatus(0, "disabled") ? 0 : 1;
    }
    if (!android::base::GetBoolProperty("ro.crypto.metadata.enabled", false) &&
        android::base::GetProperty("ro.crypto.state", "") != "encrypted") {
        return Fail("encryption_required");
    }

    const auto zram_priority = WaitForZramPriority();
    if (!zram_priority.has_value()) {
        return Fail("zram_not_ready");
    }
    if (!DisableSwap()) {
        return Fail("swapoff_failed");
    }
    if (!EnsureDataDirectory()) {
        return Fail("file_error");
    }

    const uint64_t requested_bytes = static_cast<uint64_t>(requested_mib) * kBytesPerMib;
    switch (PrepareSwapFile(requested_bytes)) {
        case PrepareResult::kOk:
            break;
        case PrepareResult::kInsufficientSpace:
            return FailPreparedSwap("insufficient_space");
        case PrepareResult::kFileError:
            return FailPreparedSwap("file_error");
        case PrepareResult::kAllocationError:
            return FailPreparedSwap("allocation_failed");
    }

    if (!RunMkswap()) {
        return FailPreparedSwap("format_failed");
    }
    if (swapon(kSwapPath, 0) != 0) {
        PLOG(ERROR) << "Failed to activate RAM Plus swap";
        return FailPreparedSwap("activation_failed");
    }

    const auto swap = FindRamPlusSwap();
    const uint64_t requested_kb = static_cast<uint64_t>(requested_mib) * kKilobytesPerMib;
    if (!swap.has_value() || swap->size_kb > requested_kb ||
        requested_kb - swap->size_kb > kSwapSizeToleranceKb) {
        return FailActiveSwap(swap.has_value() ? swap->size_kb : requested_kb,
                              "verification_failed");
    }
    if (swap->priority >= *zram_priority) {
        return FailActiveSwap(swap->size_kb, "priority_error");
    }
    if (!PublishStatus(swap->size_kb, "active")) {
        return FailActiveSwap(swap->size_kb, "activation_failed");
    }

    LOG(INFO) << "Activated " << swap->size_kb << " KB of RAM Plus swap";
    return 0;
}
