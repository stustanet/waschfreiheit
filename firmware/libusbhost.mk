vpath %.c libusbhost/src

TGT_CFLAGS += -Ilibusbhost/include
DEFS += -DUSE_STM32F4_USBH_DRIVER_FS

OBJS += usbh_core.o usbh_driver_bulkonly_storage.o usbh_lld_stm32f4.o
