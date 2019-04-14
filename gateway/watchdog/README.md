# Serial watchdog
This is a watchdog running on a Atmel/Microchip AVR microcontroller, that listens on a serial port for a specific watchdog reset message:
```
wdt_feed
```

This resets the internal watchdog timer.
Otherwise, if the watchdog timer expires, it sets an output to HIGH in order to reset a connected device.
Some time before the reset, a PREFAIL signal is asserted, in order to notifiy the connected device, that the reset by the watchdog is imminent.
