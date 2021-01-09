NAME := udataapp

$(NAME)_MBINS_TYPE := app
$(NAME)_VERSION := 1.0.2
$(NAME)_SUMMARY := uData sensor framework demo

$(NAME)_SOURCES :=   udata_example.c
$(NAME)_COMPONENTS := cli sensor udata

$(NAME)_INCLUDES += ./

ifeq ($(dtc),linkkit)
AOS_CONFIG_DTC_LINKKIT = y
AOS_CONFIG_DTC_ENABLE = y
else ifeq ($(dtc),mqtt)
AOS_CONFIG_DTC_MQTT = y
AOS_CONFIG_DTC_ENABLE = y
endif
ifeq ($(AOS_CONFIG_DTC_LINKKIT),y)

$(NAME)_SOURCES += linkkit/app_entry.c
$(NAME)_SOURCES += linkkit/linkkit_example_solo.c

$(NAME)_COMPONENTS-$(AOS_CONFIG_DTC_LINKKIT) += libiot_devmodel libiot_awss cjson
$(NAME)_COMPONENTS-$(AOS_CONFIG_DTC_MQTT||AOS_CONFIG_DTC_LINKKIT) += netmgr yloop
$(NAME)_COMPONENTS-$(AOS_CONFIG_DTC_LINKKIT&&AOS_CONFIG_DTC_USE_LWIP) += lwip

$(NAME)_INCLUDES += ./
GLOBAL_DEFINES += DTC_LINKKIT

else ifeq ($(AOS_CONFIG_DTC_MQTT),y)

$(NAME)_SOURCES    += mqtt/mqtt_example.c

$(NAME)_COMPONENTS-$(AOS_CONFIG_DTC_MQTT) += netmgr yloop libiot_mqtt
GLOBAL_DEFINES     += USE_LPTHREAD

GLOBAL_DEFINES     += DTC_MQTT
endif
