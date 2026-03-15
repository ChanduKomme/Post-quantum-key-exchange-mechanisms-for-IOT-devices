PROJECT_NAME  := sniffer
PROJECT_PATH  := $(abspath .)
PROJECT_BOARD := evb
export PROJECT_PATH PROJECT_BOARD

-include ./proj_config.mk



PROJECT_FLAGS :=
PROJECT_FLAGS += -DWITH_LWIP
# If you REALLY want CoAP as well, uncomment these:
# PROJECT_FLAGS += -DWITH_COAP
# PROJECT_FLAGS += -DWITH_COAPS
# PROJECT_FLAGS += -DCOAP_CLIENT_SUPPORT
# PROJECT_FLAGS += -DCOAP_SERVER_SUPPORT
# PROJECT_FLAGS += -DCOAP_OSCORE_SUPPORT=0
# PROJECT_FLAGS += -DCOAP_PROXY_SUPPORT=0
# PROJECT_FLAGS += -DTLS_DISABLE_ANTI_REPLAY


CFLAGS   += $(PROJECT_FLAGS)
CPPFLAGS += $(PROJECT_FLAGS)
CXXFLAGS += $(PROJECT_FLAGS)

ifeq ($(origin BL60X_SDK_PATH), undefined)
$(error ****** Please set SDK paths ******)
endif


# Network stack (LWIP + DHCP + optional DNS)
COMPONENTS_NETWORK := dns_server lwip lwip_dhcpd

# If you kept libcoap above, also add:
# COMPONENTS_NETWORK += libcoap

# BL602 system support
COMPONENTS_BLSYS   := bltime blfdt blmtd bloop looprt loopset

# VFS / filesystem
COMPONENTS_VFS     := romfs


# Core SDK + system components
INCLUDE_COMPONENTS += freertos bl602 bl602_std bl602_wifi bl602_wifidrv
INCLUDE_COMPONENTS += hal_drv vfs yloop utils cli blog blog_testc
INCLUDE_COMPONENTS += easyflash4 mbedtls-bl602 etl

# Network, system, VFS
INCLUDE_COMPONENTS += $(COMPONENTS_NETWORK)
INCLUDE_COMPONENTS += $(COMPONENTS_BLSYS)
INCLUDE_COMPONENTS += $(COMPONENTS_VFS)

# Local OLED / I2C sources are compiled from this project directory.

# This application itself
INCLUDE_COMPONENTS += $(PROJECT_NAME)
CFLAGS += -DWITH_SNIFFER


include $(BL60X_SDK_PATH)/make_scripts_riscv/project.mk

