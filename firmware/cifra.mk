vpath %.c cifra/src

TGT_CFLAGS += -Icifra/src
TGT_CFLAGS += -Icifra/src/ext

OBJS += hmac.o sha256.o
