vpath %.c FreeRTOS/FreeRTOS/Source


ifeq ($(VERSION),V1)
vpath %.c FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM3
TGT_CFLAGS += -IFreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM3
else
vpath %.c FreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM4F
TGT_CFLAGS += -IFreeRTOS/FreeRTOS/Source/portable/GCC/ARM_CM4F
endif

TGT_CFLAGS += -IFreeRTOS/FreeRTOS/Source/include

OBJS += tasks.o port.o list.o queue.o
