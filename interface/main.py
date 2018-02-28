#!/usr/bin/env python3
import asyncio
import argparse
import time
import wasch
import machinemanager


class WaschenInFreiheit:
    """
    Main management interface that runs all observer jobs and tries to make
    sense of all data available. Also here for glueing the website to status
    changes
    """

    def __init__(self, configfile, serial_port, loop=None):
        if loop is None:
            loop = asyncio.get_event_loop()
        self.loop = loop

        self.master = wasch.WaschInterface(
            serial_port,
            configfile,
            loop=self.loop)
        self.observertask = loop.create_task(self.observer())
        self.machineobserver = None
        self.startup_time = 0
        self.startup = time.time()

    async def teardown(self):
        """
        Retrieve the results of all async tasks
        """
        try:
            if self.machineobserver:
                await self.machineobserver.teardown()
        except asyncio.CancelledError:
            pass
        try:
            await self.master.teardown()
        except asyncio.CancelledError:
            pass

        try:
            if self.observertask:
                self.observertask.cancel()
                await self.observertask
        except asyncio.CancelledError:
            pass


    async def start(self):
        """
        Start the execution, afterwards the event loop should be run forever
        """
        await self.master.start()
        self.startup_time = int((time.time() - self.startup) * 1000) / 1000
        try:
            await self.master.get_node("HSH16").authping()

            self.machineobserver = machinemanager.MachineManager(
                self.master, "http://waschen.stusta.de",
                "token", loop=self.loop)
        except wasch.WaschError:
            self.master.log.error("Wasch startup has failed")

    async def observer(self):
        """
        Keep track of the internal state of the waschinterface. This is a much
        less verbose interface than the stdout, where everything is printed.
        This provides an overview over what is being done
        """
        while True:
            await asyncio.sleep(1)
            if not self.master:
                continue
            state = "*************************************************\n"
            state += "* Update time: {}\n".format(time.asctime())
            if self.master.networkmanager.next_observer_run:
                state += "* Next network observer run in {}s\n"\
                         .format(int(
                             self.master.networkmanager.next_observer_run - time.time()))
            else:
                state += "* Next network observer run: None\n"

            if self.master.networkmanager.last_observer_run:
                state += "* Last observer run {}s ago\n"\
                         .format(int(
                             time.time() - self.master.networkmanager.last_observer_run))
            else:
                state += "* Last observer run: Never\n"

            if self.startup_time:
                state += "* Startup time was: {}s\n"\
                         .format(self.startup_time)
            else:
                state += "* Startup time was: Still waiting\n"

            if self.machineobserver:
                state += "* Full network startup: {}s\n"\
                         .format(int(self.machineobserver.all_there
                                     - self.startup))
            else:
                state += "* Full network startup: Still waiting...\n"

            state += "* Master:\n"
            state += "*  - message_pending:  {}\n"\
                     .format(self.master.message_pending)
            if self.master.running_message:
                msg = self.master.running_message
                state += "*  - last_command:     {}\n".format(msg)
                state += "*     - messagestr:    {}\n".format(msg.message)
                state += "*     - retransmit:    {}\n".format(msg.retransmit)
                state += "*     - r_count:       {}\n"\
                         .format(msg.retransmit_count)
            state += "*\n"
            state += "*  - Sensors [{}]:\n".format(len(self.master.sensors))
            for _, snsor in self.master.sensors.items():
                state += "*\n"
                state += "*    # SENSOR: {} ({})\n"\
                         .format(snsor.config.name, snsor.nodeid)
                state += "*      - distance:      {}\n".format(snsor.distance)
                state += "*      - last_status:   {}\n"\
                         .format(snsor.last_status)
                state += "*      - last_update:   {}\n"\
                         .format(snsor.last_update)
                state += "*      - state:         {}\n".format(snsor.state)
                if self.master.running_message.node == snsor:
                    state += "*      - running command: {}\n"\
                             .format(self.master.running_message)

            state += "\n\n"

            with open("/tmp/wasch.state", 'w+') as statefile:
                statefile.write(state)




def main():
    loop = asyncio.get_event_loop()
    loop.set_debug(True)
    # In order to test the interface open a virtual socket using
    # `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
    # And enter its /dev/pts/xx file here

    # if you have a real serial interface, just issue the serial /dev/ttyUSBX
    # here.
    parser = argparse.ArgumentParser(description="Start the wasch-master")
    parser.add_argument("-c", "--configfile", help="configfile to use")
    parser.add_argument("-s", "--serial", help="Serial port to use")
    args = parser.parse_args()

    if args.configfile:
        configfile = args.configfile
    else:
        configfile = "nodes.json"
    if args.serial:
        serial_port = args.serial
    else:
        serial_port = "/tmp/waschfreiheit_pts"

    waschen = WaschenInFreiheit(configfile, serial_port, loop=loop)
    try:
        loop.run_until_complete(waschen.start())
        waschen.master.log.info("Done with startup")
        loop.run_forever()
    except KeyboardInterrupt:
        waschen.master.log.error("Cancelled by Keyboard.")
    finally:
        waschen.master.log.info("Starting teardown")
        loop.run_until_complete(waschen.teardown())
        waschen.master.log.info("Teardown complete."
                                "Goodbye and thanks for all the fish")
        loop.stop()

if __name__ == "__main__":
    main()
