ifndef _AXION_CONFIG_BOARD_COMMON_MK
_AXION_CONFIG_BOARD_COMMON_MK := 1

TARGET_SHIPS_AXION_KERNEL_MODULES ?= true
TARGET_SUPPORTS_KERNEL_MANAGER ?= true
TARGET_DISABLES_LIBPERF ?= true

AXION_TARGET_DEVICE := $(strip $(or $(TARGET_DEVICE),$(PRODUCT_DEVICE),$(TARGET_BOOTLOADER_BOARD_NAME),$(patsubst lineage_%,%,$(TARGET_PRODUCT)),$(patsubst axion_%,%,$(TARGET_PRODUCT))))
AXION_SOC := $(strip $(or $(AXION_SOC),$(TARGET_BOARD_PLATFORM)))

ifeq ($(TARGET_SUPPORTS_KERNEL_MANAGER),true)
ifneq ($(filter gs101 gs201 zuma zumapro,$(TARGET_BOARD_PLATFORM)),)
AXION_KM_CONFIG := $(firstword $(wildcard \
    $(DEVICE_PATH)/ax_kernel_manager.xml \
    $(COMMON_PATH)/ax_kernel_manager.xml \
    $(LOCAL_PATH)/ax_kernel_manager.xml \
    device/axion/common/prebuilts/ax_kernel_manager_$(AXION_TARGET_DEVICE).xml \
    device/axion/common/prebuilts/ax_kernel_manager_$(TARGET_BOARD_PLATFORM).xml))
else
AXION_KM_CONFIG := $(firstword $(wildcard \
    $(DEVICE_PATH)/ax_kernel_manager.xml \
    $(COMMON_PATH)/ax_kernel_manager.xml \
    $(LOCAL_PATH)/ax_kernel_manager.xml \
    device/axion/common/prebuilts/ax_kernel_manager_$(AXION_TARGET_DEVICE).xml))
endif

ifneq ($(AXION_KM_CONFIG),)
PRODUCT_COPY_FILES += \
    $(AXION_KM_CONFIG):$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/ax_kernel_manager.xml
endif
endif

AXION_PLATFORM_PROP := $(wildcard device/axion/common/platform/$(AXION_SOC)/props/ax_$(AXION_SOC).prop)
ifneq ($(AXION_PLATFORM_PROP),)
TARGET_PRODUCT_PROP += $(AXION_PLATFORM_PROP)
endif

PRODUCT_COPY_FILES += \
    device/axion/common/init/init.axion.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.axion.rc

ifeq ($(TARGET_SHIPS_AXION_KERNEL_MODULES),true)
PRODUCT_COPY_FILES += \
    device/axion/common/init/init.axion.modules.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.axion.modules.rc

ifeq ($(TARGET_USE_BORE_SCHED),true)
PRODUCT_COPY_FILES += \
    device/axion/common/init/init.axion.modules.bore.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.axion.modules.bore.rc
endif

ifeq ($(TARGET_USE_LATENCY_SCHED),true)
PRODUCT_COPY_FILES += \
    device/axion/common/init/init.axion.modules.latency.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.axion.modules.latency.rc
endif
endif

AXION_PLATFORM_INIT_RC := $(firstword $(wildcard \
    $(DEVICE_PATH)/ax_init_$(AXION_TARGET_DEVICE).rc \
    $(COMMON_PATH)/ax_init_$(AXION_TARGET_DEVICE).rc \
    device/axion/common/init/ax_init_$(AXION_TARGET_DEVICE).rc \
    device/axion/common/init/ax_init_$(AXION_SOC).rc))
ifneq ($(AXION_PLATFORM_INIT_RC),)
PRODUCT_COPY_FILES += \
    $(AXION_PLATFORM_INIT_RC):$(TARGET_COPY_OUT_VENDOR)/etc/init/ax_init_$(AXION_SOC).rc
endif

AXION_PERF_THERMAL_CONFIG := $(firstword $(wildcard \
    $(DEVICE_PATH)/ax_perf_thermal.xml \
    $(COMMON_PATH)/ax_perf_thermal.xml \
    $(LOCAL_PATH)/ax_perf_thermal.xml \
    device/axion/common/prebuilts/ax_perf_thermal_$(AXION_TARGET_DEVICE).xml \
    device/axion/common/prebuilts/ax_perf_thermal_$(AXION_SOC).xml))
ifneq ($(AXION_PERF_THERMAL_CONFIG),)
PRODUCT_COPY_FILES += \
    $(AXION_PERF_THERMAL_CONFIG):$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/ax_perf_thermal.xml
endif

endif
