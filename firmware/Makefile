BUILDDIR = build
BINDIR = bin

ifeq ($(VERSION),V1)
LIBNAME	= opencm3_stm32f1
DEFS += -DSTM32F1 -DWASCHV1
PROJECT = wasch_v1
LDSCRIPT = libopencm3/lib/stm32/f1/stm32f103xb.ld
OOCD_TARGET	= stm32f1x
FP_FLAGS	= -mfpu=vfp -msoft-float
ARCH_FLAGS	= -mcpu=cortex-m3 -mthumb -mno-thumb-interwork $(FP_FLAGS)
else
LIBNAME	= opencm3_stm32f4
DEFS += -DSTM32F4 -DWASCHV2
PROJECT = wasch_v2
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
