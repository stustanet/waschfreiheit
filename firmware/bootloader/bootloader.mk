FILES += bootloader usb_storage_helper diskio pff status

vpath %.c bootloader
vpath %.c bootloader/pff

OBJS += $(addsuffix .o,$(FILES))
TGT_CFLAGS += -Ibootloader -Ibootloader/pff

