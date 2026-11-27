AXION_COMMON_PATH := device/axion/common
AXION_COMMON_MODULE_ROOT := $(AXION_COMMON_PATH)/kernel/modules

AXION_COMMON_KERNEL_MODULES := \
    ax_dragonite

ifeq ($(TARGET_USE_BORE_SCHED),false)
ifneq ($(wildcard $(TARGET_KERNEL_SOURCE)/include/trace/hooks/bore.h $(KERNEL_SRC)/include/trace/hooks/bore.h $(TARGET_KERNEL_HEADERS)/include/trace/hooks/bore.h out-kernel/$(TARGET_KERNEL_PLATFORM_SOURCE)/aosp/include/trace/hooks/bore.h),)
TARGET_USE_BORE_SCHED := true
endif
endif

ifeq ($(TARGET_USE_BORE_SCHED),true)
AXION_COMMON_KERNEL_MODULES += \
    bore_sched
BOARD_VENDOR_KERNEL_MODULES_LOAD := \
    $(filter-out bore_sched.ko,$(BOARD_VENDOR_KERNEL_MODULES_LOAD)) \
    bore_sched.ko
else
BOARD_VENDOR_KERNEL_MODULES_LOAD := \
    $(filter-out bore_sched.ko,$(BOARD_VENDOR_KERNEL_MODULES_LOAD))
endif

ifeq ($(TARGET_USE_LATENCY_SCHED),false)
ifneq ($(wildcard $(TARGET_KERNEL_SOURCE)/include/trace/hooks/latency.h $(KERNEL_SRC)/include/trace/hooks/latency.h $(TARGET_KERNEL_HEADERS)/include/trace/hooks/latency.h out-kernel/$(TARGET_KERNEL_PLATFORM_SOURCE)/aosp/include/trace/hooks/latency.h),)
TARGET_USE_LATENCY_SCHED := true
endif
endif

ifeq ($(TARGET_USE_LATENCY_SCHED),true)
AXION_COMMON_KERNEL_MODULES += \
    latency_sched
BOARD_VENDOR_KERNEL_MODULES_LOAD := \
    $(filter-out latency_sched.ko,$(BOARD_VENDOR_KERNEL_MODULES_LOAD)) \
    latency_sched.ko
else
BOARD_VENDOR_KERNEL_MODULES_LOAD := \
    $(filter-out latency_sched.ko,$(BOARD_VENDOR_KERNEL_MODULES_LOAD))
endif

ifneq ($(strip $(TARGET_KERNEL_SOURCE)),)
ifeq ($(strip $(TARGET_PREBUILT_KERNEL)),)
ifneq ($(wildcard $(TARGET_KERNEL_SOURCE)/Makefile),)
AXION_COMMON_KERNEL_VERSION := $(shell awk \
    '/^VERSION =/{version=$$3} /^PATCHLEVEL =/{patch=$$3} END{if (version && patch) print version "." patch}' \
    $(TARGET_KERNEL_SOURCE)/Makefile)
AXION_COMMON_KERNEL_MAJOR := $(firstword $(subst ., ,$(AXION_COMMON_KERNEL_VERSION)))

ifeq ($(strip $(TARGET_KERNEL_EXT_MODULE_ROOT)),)
TARGET_KERNEL_EXT_MODULE_ROOT := $(AXION_COMMON_MODULE_ROOT)
endif

ifeq ($(TARGET_KERNEL_EXT_MODULE_ROOT),$(AXION_COMMON_MODULE_ROOT))
TARGET_KERNEL_EXT_MODULES += $(foreach module,$(AXION_COMMON_KERNEL_MODULES),$(module):kbuild)
endif
endif
endif
endif
