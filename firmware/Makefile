TARGET ?= build

ifeq ($(VERSION),V1)
	ifeq ($(NODE),BOOTLOADER)
		$(error Bootloader not available for V1 boards)
	else ifeq ($(NODE),MASTER)
		PROJECT=master_v1
	else
		PROJECT=sensor_v1
	endif
else
	ifeq ($(NODE),BOOTLOADER)
		PROJECT=bootloader
	else ifeq ($(NODE),MASTER)
		PROJECT=master_v2
	else
		PROJECT=sensor_v2
		USE_BOOTLOADER ?= TRUE
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
	OOCD_TARGET	= stm32f4x
	FP_FLAGS	= -mfloat-abi=hard -mfpu=fpv4-sp-d16
	ARCH_FLAGS	= -mthumb -mcpu=cortex-m4 $(FP_FLAGS)

	ifeq ($(NODE),BOOTLOADER)
		LDSCRIPT = stm32f401rbt6.ld
		#DEFS += -DLUSBH_USART_DEBUG
		include i2s_rgb.mk
		include libusbhost.mk
		OPT ?= -Os
	else
		ifeq ($(NODE),MASTER)
			LDSCRIPT = stm32f401rbt6.ld
		else
			ifeq ($(USE_BOOTLOADER),TRUE)
				LDSCRIPT = stm32f401rbt6_bldr.ld
				DEFS += -DUSE_BOOTLOADER
			else
				LDSCRIPT = stm32f401rbt6.ld
			endif
			#DEFS += -DLUSBH_USART_DEBUG
			include libusbhost.mk
			include fatfs.mk
			include i2s_rgb.mk
		endif
	endif

endif

ifeq ($(NODE),MASTER)
	DEFS += -DCLI_PROMPT="\"MASTER>\\n\""
endif

ifeq ($(NODE),BOOTLOADER)
	include bootloader/bootloader.mk
else
	include source/node.mk
	include freertos.mk
	include cifra.mk
endif

include tinyprintf.mk

OOCD = openocd
OOCD_INTERFACE = stlink-v2
BINARY = bin/$(PROJECT)
$(shell mkdir -p $(BINDIR) $(BUILDDIR))
include ./rules.mk


all: TARGET=build
all: sensor_v1 sensor_v2 master_v1 master_v2 bootloader_v2

clean: TARGET=_clean
clean: sensor_v1 sensor_v2 master_v1 master_v2 bootloader_v2

flash_sv1: sensor_v1
flash_sv1:
	@$(MAKE) NODE=SENSOR VERSION=V1 flash

flash_sv2: sensor_v2
flash_sv2:
	@$(MAKE) NODE=SENSOR VERSION=V2 flash

flash_mv1: master_v1
flash_mv1:
	@$(MAKE) NODE=MASTER VERSION=V1 flash

flash_mv2: master_v2
flash_mv2:
	@$(MAKE) NODE=MASTER VERSION=V2 flash

flash_blv2: bootloader_v2
flash_blv2:
	@$(MAKE) NODE=BOOTLOADER VERSION=V2 flash

sensor_v1:
	@$(MAKE) NODE=SENSOR VERSION=V1 $(TARGET)

sensor_v2:
	@$(MAKE) NODE=SENSOR VERSION=V2 $(TARGET)

master_v1:
	@$(MAKE) NODE=MASTER VERSION=V1 $(TARGET)

master_v2:
	@$(MAKE) NODE=MASTER VERSION=V2 $(TARGET)

bootloader_v2:
	@$(MAKE) NODE=BOOTLOADER VERSION=V2 $(TARGET)
