NAME := otaapp

$(NAME)_MBINS_TYPE := app
$(NAME)_VERSION := 1.0.2
$(NAME)_SUMMARY := OTA demo app for developers

OTAAPP_CONFIG_TEST_LOOP ?= n
loop ?= $(OTAAPP_CONFIG_TEST_LOOP)

$(NAME)_SOURCES := otaapp.c
$(NAME)_COMPONENTS := netmgr cli ota libiot_mqtt
$(NAME)_COMPONENTS-$(OTAAPP_CONFIG_USE_LWIP) += lwip

ifeq ($(loop),1)
$(NAME)_DEFINES     += TEST_LOOP
endif

#ifeq ($(strip $(CONFIG_SYSINFO_DEVICE_NAME)), $(filter $(CONFIG_SYSINFO_DEVICE_NAME), developerkit))
#AOS_DEVELOPERKIT_ENABLE_OTA := 1
#endif

$(NAME)_INCLUDES += ./
