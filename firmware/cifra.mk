vpath %.c cifra/src

TGT_CFLAGS += -Icifra/src
TGT_CFLAGS += -Icifra/src/ext
TGT_CFLAGS += -Dtypeof=__typeof__

OBJS += hmac.o sha256.o blockwise.o chash.o
