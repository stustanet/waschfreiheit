ifeq ($(NODE),MASTER)
DEFS += -DMASTER
include source/master/master.mk
else
include source/sensor/sensor.mk
endif

FILES += main cli serial_getchar_dma sx127x utils commands_common meshnw auth

vpath %.c source

OBJS += $(addsuffix .o,$(FILES))
TGT_CFLAGS += -Iinclude

