FILES = main
#FILES = main auth debug_command_queue debug_file_logger i2s_rgb led_status meshnw sensor_config sensor_node serial_getchar_dma state_estimation usb_storage_helper utils

vpath %.c source

OBJS += $(addsuffix .o,$(FILES))
TGT_CFLAGS += -Iinclude
