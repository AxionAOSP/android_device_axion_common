include device/axion/common/config/soc_map.mk
include device/axion/common/config/flags.mk

ifneq ($(filter gs101 gs201 zuma zumapro,$(TARGET_BOARD_PLATFORM)),)
include device/axion/common/config/product/tensor.mk
endif

include device/axion/common/config/board/common.mk
include device/axion/common/config/properties.mk
include device/axion/common/config/version.mk
include device/axion/common/config/dexpreopt.mk
include device/axion/common/config/packages.mk
include device/axion/common/config/ramplus/ramplus.mk

PRODUCT_PACKAGE_OVERLAYS += device/axion/common/overlay

PRODUCT_SOONG_NAMESPACES += device/axion/common
