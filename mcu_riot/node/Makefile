APPLICATION = node

BOARD ?= bluepill

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += xtimer
USEMODULE += sx1276
USEMODULE += hashes


FEATURES_REQUIRED ?= periph_spi
FEATURES_REQUIRED ?= periph_gpio
FEATURES_REQUIRED ?= periph_adc

node := $(strip $(NODE))
ifeq ($(node),MASTER)
	CFLAGS += -DMASTER
endif

CFLAGS += -DDEVELHELP

INCLUDES += -I$(CURDIR)/header

include $(RIOTBASE)/Makefile.include
