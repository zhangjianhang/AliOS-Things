NAME := yts

$(NAME)_MBINS_TYPE := app
$(NAME)_VERSION := 1.0.0
$(NAME)_SUMMARY := AliOS Things test suite
$(NAME)_SOURCES := main.c

$(NAME)_COMPONENTS := testcase ulog vfs yloop
$(NAME)_COMPONENTS-$(AOS_BOARD_LINUXHOST||AOS_BOARD_ARMHFLINUX) += lwip netmgr fatfs
$(NAME)_COMPONENTS-$(!AOS_BOARD_LINUXHOST&&!AOS_BOARD_ARMHFLINUX) += cli
$(NAME)_COMPONENTS-$(EN_UAGENT_TEST) += uagent_test

$(NAME)_CFLAGS += -Wall -Werror -Wno-unused-variable

ifneq (,$(findstring linux, $(BUILD_STRING)))
GLOBAL_LDFLAGS += -lreadline -lncurses -lrt
GLOBAL_DEFINES += CONFIG_AOS_MESHYTS DEBUG YTS_LINUX
endif

$(NAME)_INCLUDES += ./
