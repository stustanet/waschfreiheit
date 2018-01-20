import sensor
import asyncio

class MachineManager:
    def __init__(self, master, server, credentials, loop=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.leds = [sensor.LED.OFF] * len(master._sensors)
        self.master = master
        self.server = server
        print("MachineManager status subscribe")
        self.master.status_subscribe(self.status_update)

        self.status_event = asyncio.Event(loop=loop)
        self.loop.create_task(self.send_all_status())

    def status_update(self, nodeid, status):
        print("status update")
        self.status_event.set()

    def status_to_color(self, status):
        status = int(status)
        if status == 3:
            return sensor.LED.RED
        elif status in (2, 1):
            return sensor.LED.YELLOW
        elif status == 0:
            return sensor.LED.GREEN
        else:
            return sensor.LED.BLUE

    async def send_all_status(self):
        while True:
            await self.status_event.wait()
            self.status_event.clear()

            await asyncio.sleep(0.1)

            leds = []
            for sname in ['HSH2', 'HSH7', 'HSH10', 'HSH16']:
                try:
                    snsor = self.master.get_node(sname)
                    if snsor.last_status:
                        leds.append(self.status_to_color(snsor.last_status))
                    else:
                        leds.append(sensor.LED.OFF)
                except KeyError:
                    leds.append(sensor.LED.OFF)

            print("leds:", leds)
            if leds != self.leds:
                self.leds = leds
                for snsor in self.master._sensors.values():
                    await snsor.led(leds)
            else:
                print("LEDs have not changed")

