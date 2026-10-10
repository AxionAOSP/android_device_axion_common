AXION_TARGET_DEVICE := $(strip $(or $(TARGET_DEVICE),$(PRODUCT_DEVICE),$(TARGET_BOOTLOADER_BOARD_NAME),$(patsubst lineage_%,%,$(TARGET_PRODUCT)),$(patsubst axion_%,%,$(TARGET_PRODUCT))))

ifeq ($(TARGET_BOARD_PLATFORM),)
ifneq ($(filter oriole raven bluejay,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := gs101
else ifneq ($(filter panther cheetah lynx felix tangorpro,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := gs201
else ifneq ($(filter shiba husky akita,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := zuma
else ifneq ($(filter caiman komodo tokay comet,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := zumapro
else ifneq ($(filter larry,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := holi
else ifneq ($(filter violet,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := sm6150
else ifneq ($(filter alioth,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := kona
else ifneq ($(filter garnet,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := sm7435
else ifneq ($(filter avicii,$(AXION_TARGET_DEVICE)),)
    TARGET_BOARD_PLATFORM := lito
endif
endif

AXION_PLATFORM := $(strip $(TARGET_BOARD_PLATFORM))
AXION_SOC := $(AXION_PLATFORM)
