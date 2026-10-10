# Tensor feature flags
HBM_SUPPORTED := true
HBM_NODE := /sys/class/backlight/panel0-backlight/hbm_mode
TORCH_STR_SUPPORTED := true
TARGET_NEEDS_DOZE_FIX := true
TARGET_ENABLES_IMS_OVERRIDES := true
TARGET_TOUCH_BOOST_SUPPORTED := true
TARGET_INCLUDE_AXFX := true
TARGET_PREBUILT_GOOGLE_CAMERA := true
TARGET_DOZE_TAP_PULSE_SUPPORTED := true
TARGET_DOZE_PICKUP_PULSE_SUPPORTED := true

-include vendor/google/camera/config.mk

PRODUCT_PACKAGES += ax_perf

ifneq ($(TARGET_BOARD_PLATFORM),)
PRODUCT_SOONG_NAMESPACES += device/axion/common/platform/$(TARGET_BOARD_PLATFORM)
endif

PRODUCT_PROPERTY_OVERRIDES += \
    persist.dbg.volte_avail_ovr=1 \
    persist.dbg.vt_avail_ovr=1 \
    persist.dbg.wfc_avail_ovr=1 \
    persist.rcs.supported=1
