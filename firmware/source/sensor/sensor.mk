FILES += led_ws2801 sensor_config state_estimation sensor_adc sensor_node
#FILES = main auth debug_command_queue debug_file_logger i2s_rgb led_status meshnw sensor_config sensor_node serial_getchar_dma state_estimation usb_storage_helper utils

vpath %.c source/sensor

TGT_CFLAGS += -Iinclude/sensor
