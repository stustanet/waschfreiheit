APPLICATION = node

BOARD ?= wasch_v1

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += xtimer
USEMODULE += sx1276
USEMODULE += hashes
USEMODULE += periph_adc
USEMODULE += periph_flashpage


FEATURES_REQUIRED ?= periph_spi
FEATURES_REQUIRED ?= periph_gpio
FEATURES_REQUIRED ?= periph_adc
FEATURES_REQUIRED ?= periph_flashpage

node := $(strip $(NODE))
ifeq ($(node),MASTER)
	CFLAGS += -DMASTER
endif

CFLAGS += -DDEVELHELP

INCLUDES += -I$(CURDIR)/header

include $(RIOTBASE)/Makefile.include
