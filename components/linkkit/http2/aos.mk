NAME := libiot_http2stream

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION := 3.0.2
$(NAME)_SUMMARY := http2 stream service

$(NAME)_SOURCES := http2_api.c iotx_http2.c
$(NAME)_SOURCES-$(FS_ENABLED) += http2_upload_api.c

$(NAME)_COMPONENTS := libiot_wrappers libiot_infra libiot_nghttp2

$(NAME)_INCLUDES += .


