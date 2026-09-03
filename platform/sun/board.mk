PRODUCT_SOONG_NAMESPACES += device/axion/common/platform/sun

# sm8735 is built from a Bazel/kleaf tree, and nothing in this tree consumes
# TARGET_KERNEL_EXT_MODULE_ROOT, so the axion modules (ax_atcm, ax_boost, ...) are not
# compiled in: /proc/ax_atcm is absent at runtime and the ATMC floors of
# prebuilts/ax_perf_thermal_sun.xml have no kernel sink yet. The kernel manager itself
# is userspace only and works as expected.
include device/axion/common/config/board/common.mk
