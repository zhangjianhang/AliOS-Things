NAME := bleadv

$(NAME)_MBINS_TYPE := app
$(NAME)_VERSION := 1.0.2
$(NAME)_SUMMARY := Ble adv example.
$(NAME)_SOURCES     := bleadv_app.c

$(NAME)_COMPONENTS  += bt_host cli
GLOBAL_DEFINES      += AOS_NO_WIFI

$(NAME)_INCLUDES += ../
