#!/usr/bin/env python3
import wasch
import asyncio

if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    master = wasch.WaschInterface("/dev/ttyUSB0", loop=loop)
    node = master.node(1)

    master.status_subscribe(lambda n, s: print("node:", n,"status", s))

    loop.run_until_complete(node.ping())
    loop.run_until_complete(master.send_raw("###status1-128"))
    loop.run_forever()

