#!/usr/bin/env python3
import wasch
import asyncio

async def run():

    # Configure the master
    await master.sensor_routes({
        1:1,
        2:2
    })

    node = await master.node(2)
    master.status_subscribe(lambda n, s: print("node:", n,"status", s))
    try:
        await node.connect(0, 5)
        await node.authping()
    except wasch.WaschException:
        print("Wasch has failed")




if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    # In order to test the interface open a virtual socket using
    # `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
    # And enter its /dev/pts/xx file here

    # if you have a real serial interface, just issue the serial /dev/ttyUSBX
    # here.

    master = wasch.WaschInterface("/dev/ttyUSB0", loop=loop)

    loop.run_until_complete(run())

    loop.run_forever()

