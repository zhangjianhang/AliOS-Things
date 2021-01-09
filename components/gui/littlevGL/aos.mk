NAME := littlevGL

$(NAME)_MBINS_TYPE := framework
$(NAME)_VERSION    := 1.0.2
$(NAME)_SUMMARY    := a lightweight GUI lib

#default gcc
ifeq ($(COMPILER),)
$(NAME)_CFLAGS      += -Wall -Werror
else ifeq ($(COMPILER),gcc)
$(NAME)_CFLAGS      += -Wall -Werror
else ifeq ($(COMPILER),armcc)
GLOBAL_DEFINES      += __BSD_VISIBLE
endif

GLOBAL_INCLUDES     += .
GLOBAL_DEFINES      += AOS_COMP_LITTLEVGL

include ./components/gui/littlevGL/src/src.mk
