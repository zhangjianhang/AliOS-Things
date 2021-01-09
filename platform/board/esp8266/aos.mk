NAME := board_esp8266

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION    := 1.0.2
$(NAME)_SUMMARY    := configuration for board esp8266
MODULE             := 1062
HOST_ARCH          := xtensa
HOST_MCU_FAMILY    := mcu_esp8266
SUPPORT_MBINS      := no

# todo: remove these after rhino/lwip ready
osal ?= rhino

$(NAME)_COMPONENTS += $(HOST_MCU_FAMILY) kernel_init network

CONFIG_SYSINFO_PRODUCT_MODEL := ALI_AOS_ESP8266
CONFIG_SYSINFO_DEVICE_NAME   := ESP8266

GLOBAL_CFLAGS += -DSYSINFO_PRODUCT_MODEL=\"$(CONFIG_SYSINFO_PRODUCT_MODEL)\"
GLOBAL_CFLAGS += -DSYSINFO_DEVICE_NAME=\"$(CONFIG_SYSINFO_DEVICE_NAME)\"

#for activation handle
GLOBAL_CFLAGS += -DBOARD_ESP8266

GLOBAL_INCLUDES += .
$(NAME)_SOURCES := config/partition_conf.c startup/board.c startup/startup.c

GLOBAL_DEFINES += LOCAL_PORT_ENHANCED_RAND

ifeq ($(APP), yts)
GLOBAL_DEFINES += CLI_CONFIG_STACK_SIZE=4096
endif

ifeq ($(osal),freertos)

else
$(NAME)_SOURCES   += config/k_config.c
endif

GLOBAL_INCLUDES += ./config/ ./drivers/

ifeq ($(SUPPORT_ESP8285),yes)
GLOBAL_LDS_FILES += platform/board/esp8266/ld/eagle.app.v6.new_8285.1024.app1.ld
else
GLOBAL_LDS_FILES += platform/board/esp8266/ld/eagle.app.v6.new.1024.app1.ld
endif

GLOBAL_LDFLAGS   += -Lplatform/board/esp8266/ld
