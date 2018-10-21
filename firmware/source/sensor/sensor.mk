FILES += led_ws2801 sensor_config state_estimation sensor_adc sensor_node sensor_main

vpath %.c source/sensor

TGT_CFLAGS += -Iinclude/sensor
