vpath %.c fatfs/source

TGT_CFLAGS += -Ifatfs/source

OBJS += diskio.o ff.o ffsystem.o ffunicode.o