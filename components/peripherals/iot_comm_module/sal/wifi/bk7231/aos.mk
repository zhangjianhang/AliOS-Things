NAME := device_sal_bk7231

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION := 1.0.2
$(NAME)_SUMMARY := sal hal implemenation for bk7231

GLOBAL_CFLAGS += -D__SAL_DEV_NAME=bk7231

$(NAME)_SOURCES += bk7231.c \
                   wifi_atcmd_bk7231.c


GLOBAL_INCLUDES += ./

$(NAME)_COMPONENTS += at yloop netmgr
