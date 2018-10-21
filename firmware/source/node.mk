ifeq ($(NODE),MASTER)
include source/master/master.mk
else
include source/sensor/sensor.mk
endif

FILES += main cli serial_getchar_dma sx127x utils commands_common meshnw auth
#FILES = main auth debug_command_queue debug_file_logger i2s_rgb led_status meshnw sensor_config sensor_node serial_getchar_dma state_estimation usb_storage_helper utils

vpath %.c source

OBJS += $(addsuffix .o,$(FILES))
TGT_CFLAGS += -Iinclude

