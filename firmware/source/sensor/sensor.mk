FILES += sensor_config state_estimation sensor_adc sensor_node sensor_main led_status

ifeq ($(VERSION),V1)
FILES += led_ws2801
FILES += stm32f1_flasher
else
FILES += i2s_rgb
FILES += usb_storage_helper
endif

vpath %.c source/sensor

TGT_CFLAGS += -Iinclude/sensor
