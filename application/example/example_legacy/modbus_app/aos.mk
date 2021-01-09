NAME := modbus_app

$(NAME)_MBINS_TYPE := app
$(NAME)_VERSION := 1.0.1
$(NAME)_SUMMARY := This example is an demo of using modbus master stack
$(NAME)_SOURCES += modbus_app.c

GLOBAL_DEFINES += AOS_NO_WIFI

$(NAME)_COMPONENTS := yloop cli mbmaster

$(NAME)_INCLUDES += ./
