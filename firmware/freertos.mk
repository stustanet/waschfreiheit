vpath %.c FreeRTOS/FreeRTOS/Source
vpath %.c FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM4F

TGT_CFLAGS += -IFreeRTOS/FreeRTOS/Source/include
TGT_CFLAGS += -IFreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM4F

OBJS += tasks.o port.o list.o queue.o
