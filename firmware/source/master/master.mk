FILES += master_config master_sensorconnection master_node master_main host_watchdog

vpath %.c source/master

TGT_CFLAGS += -Iinclude/master
