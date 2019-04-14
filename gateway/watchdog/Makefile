MCU        = attiny2313
FCPU       = 11059200

OUT        = watchdog
OBJECTS    = main.o

CC         = avr-gcc
LD         = avr-gcc
OCPY       = avr-objcopy
CFLAGS     = -g -Os -mmcu=$(MCU) -DF_CPU=$(FCPU)UL
LDFLAGS    = -g -mmcu=$(MCU)


all: $(OUT).hex

$(OUT).hex: $(OUT).elf
	$(OCPY) -j .text -j .data -O ihex $(OUT).elf $@

$(OUT).elf: $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<


clean:
	rm -f $(OBJECTS)
	rm -f $(OUT).elf
	rm -f $(OUT).hex

flash: all
	avrdude -v -p $(MCU) -c usbasp-clone -P usb -U flash:w:$(OUT).hex
