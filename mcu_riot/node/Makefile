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

CFLAGS += -DDEVELHELP
QUIET = 1

INCLUDES += -I$(CURDIR)/header

include $(RIOTBASE)/Makefile.include
