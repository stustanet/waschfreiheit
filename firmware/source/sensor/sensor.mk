FILES += sensor_config state_estimation sensor_adc sensor_node sensor_main led_status

ifeq ($(VERSION),V1)
FILES += led_ws2801
FILES += stm32f1_flasher
else
FILES += usb_storage_helper debug_command_queue debug_file_logger storage_manager frequency_sensor test_switch
endif

vpath %.c source/sensor

TGT_CFLAGS += -Iinclude/sensor
