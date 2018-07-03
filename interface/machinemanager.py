import asyncio
import time

from . import sensor
from . import wasch


class MachineManager:
    """
    Manage the machines, keep track of their states and inform the website
    that stuff has changed.
    Also manage the LEDs on the nodes of the machines themselves.
    """

    def __init__(self, master, host, credentials, loop=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.leds = [sensor.LED.OFF] * len(master.sensors)
        self.master = master
        self.host = host
        self.credentials = credentials
        self.master.status_subscribe(self.status_update)
        self.all_there = 0

    async def teardown(self):
        """
        Clean up your asyncio!
        """
        pass

    async def status_update(self, nodeid, status):
        """
        This is the callback to the updated status. It is called whenever
        a node decides to send a status message.
        """
        await self.update_sensors()
        #await self.update_website()

    def status_to_color(self, status):
        """
        The sensor sends values as bitmasks regarding the occupation of the
        machines, i.e. 0: both available, 1, 2: one is used, 3 both used
        """
        status = int(status)
        if status == 3:
            return sensor.LED.RED
        if status in (2, 1):
            return sensor.LED.YELLOW
        if status == 0:
            return sensor.LED.GREEN
        return sensor.LED.BLUE

    async def update_website(self):
        """
        Send info regarding the occupation to the website
        TODO: Actually do it
        """
        pass

    async def update_sensors(self):
        """
        Send status updates whenever a status message is triggered
        """

        while self.master.networkmanager.network_recovery_in_progress:
            await self.master.networkmanager.network_recovery_event.wait()

        leds = []
        all_ok = True
        # These could be read from the config
        for sname in ['HSH2', 'HSH7', 'HSH10', 'HSH16']:
            try:
                snsor = self.master.get_node(sname)
                if snsor.state != 'connected':
                    self.master.log.info("Skipping node %s because it is in "
                                         "state %s", snsor.config.name,
                                         snsor.state)
                    all_ok = False
                    leds.append(sensor.LED.OFF)
                    continue
                if snsor.last_status:
                    leds.append(self.status_to_color(snsor.last_status))
                else:
                    all_ok = False
                    leds.append(sensor.LED.OFF)
            except KeyError:
                all_ok = False
                leds.append(sensor.LED.OFF)

        if all_ok and self.all_there == 0:
            self.all_there = time.time()

        if leds != self.leds:
            self.master.log.info("Setting leds to %s", leds)
            self.leds = leds
            for snsor in self.master.sensors.values():
                for _ in range(10):
                    if snsor.state != 'failed':
                        try:
                            await snsor.led(leds)
                            break
                        except wasch.WaschOperationInterrupted:
                            pass
                    else:
                        break
