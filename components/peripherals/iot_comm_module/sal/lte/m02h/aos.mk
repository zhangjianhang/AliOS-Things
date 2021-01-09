NAME := device_sal_m02h

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION := 1.0.1
$(NAME)_SUMMARY := sal hal implemenation for m02h

GLOBAL_CFLAGS += -D__SAL_DEV_NAME=m02h

$(NAME)_SOURCES += m02h.c

GLOBAL_INCLUDES += ./

$(NAME)_COMPONENTS += at yloop
