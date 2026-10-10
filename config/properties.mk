TARGET_PRODUCT_PROP += \
    device/axion/common/config/defaults_common.prop

PRODUCT_PRODUCT_PROPERTIES += \
    persist.sys.ax_chg_bypass=$(BYPASS_CHARGE_SUPPORTED) \
    persist.sys.ax_hbm_supp=$(HBM_SUPPORTED) \
    persist.sys.ax_hbm_file=$(HBM_NODE) \
    persist.sys.ax_ims_ovrrde=$(TARGET_ENABLES_IMS_OVERRIDES) \
    persist.sys.ax_touch_boost=$(TARGET_TOUCH_BOOST_SUPPORTED) \
    persist.sys.ax_doze_tap=$(TARGET_DOZE_TAP_PULSE_SUPPORTED) \
    persist.sys.ax_doze_dt2p=$(TARGET_DOZE_DOUBLE_TAP_PULSE_SUPPORTED) \
    persist.sys.ax_doze_pickup=$(TARGET_DOZE_PICKUP_PULSE_SUPPORTED) \
    persist.sys.ax_doze_fps=$(TARGET_DOZE_SIDE_FPS_PULSE_SUPPORTED) \
    persist.sys.vk_use_ogl_for_media=false \
    persist.sys.ax_disable_pwrhal=$(TARGET_DISABLES_LIBPERF)
