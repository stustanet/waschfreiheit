#!/usr/bin/env python3
import wasch
import asyncio

if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    # In order to test the interface open a virtual socket using
    # `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
    # And enter its /dev/pts/xx file here

    # if you have a real serial interface, just issue the serial /dev/ttyUSBX
    # here.

    master = wasch.WaschInterface("/dev/ttyUSB0", loop=loop)
    node = master.node(1)
    master.status_subscribe(lambda n, s: print("node:", n,"status", s))

    loop.run_until_complete(node.ping())
    loop.run_until_complete(master.send_raw("###status1-128"))
    loop.run_forever()

