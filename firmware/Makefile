
TARGET ?= build

ifeq ($(VERSION),V1)
ifneq ($(NODE),MASTER)
PROJECT=sensor_v1
else
PROJECT=master_v1
endif
else
ifneq ($(NODE),MASTER)
PROJECT=sensor_v2
else
PROJECT=master_v2
endif
endif

BINDIR = bin/$(PROJECT)
BUILDDIR = build/$(PROJECT)

ifeq ($(VERSION),V1)
LIBNAME	= opencm3_stm32f1
DEFS += -DSTM32F1 -DWASCHV1
LDSCRIPT = libopencm3/lib/stm32/f1/stm32f103xb.ld
OOCD_TARGET	= stm32f1x
FP_FLAGS	= -mfpu=vfp -msoft-float
ARCH_FLAGS	= -mcpu=cortex-m3 -mthumb -mno-thumb-interwork $(FP_FLAGS)
else
LIBNAME	= opencm3_stm32f4
DEFS += -DSTM32F4 -DWASCHV2
LDSCRIPT = stm32f401rbt6.ld
OOCD_TARGET	= stm32f4x
FP_FLAGS	= -mfloat-abi=hard -mfpu=fpv4-sp-d16
ARCH_FLAGS	= -mthumb -mcpu=cortex-m4 $(FP_FLAGS)

ifneq ($(NODE),MASTER)
#DEFS += -DLUSBH_USART_DEBUG
include libusbhost.mk
include fatfs.mk
endif

endif

include source/node.mk
include freertos.mk
include tinyprintf.mk
include cifra.mk


OOCD = openocd
OOCD_INTERFACE = stlink-v2
BINARY = bin/$(PROJECT)
$(shell mkdir -p $(BINDIR) $(BUILDDIR))
include ./rules.mk


all: TARGET=build
all: sensor_v1 sensor_v2 master_v1 master_v2

clean: TARGET=_clean
clean: sensor_v1 sensor_v2 master_v1 master_v2

sensor_v1:
	@$(MAKE) NODE=SENSOR VERSION=V1 $(TARGET)

sensor_v2:
	@$(MAKE) NODE=SENSOR VERSION=V2 $(TARGET)

master_v1:
	@$(MAKE) NODE=MASTER VERSION=V1 $(TARGET)

master_v2:
	@$(MAKE) NODE=MASTER VERSION=V2 $(TARGET)
