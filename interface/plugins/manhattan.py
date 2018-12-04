"""
Implementation for the open switch for the Manhattan bar
"""

import time
import math
from functools import partial

from node import Node
from message import MessageCommand
from plugins.waschen import WaschSensor

import urllib.request


# There is no reason why someone would like to have other params than this for this type of sensor,
# so we just hard-code this values here
manhattan_sensor_config = ("0,0,1", "0,16500,0,0,0,0,-16000,0,0,0,0,0", "3,3,3,3", "1,1")
manhattan_sensor_channels = 2
manhattan_sensor_samplerate = 500

class ManhattanNode(Node):
    """
    Implements a ManhattanNode to connect to the status switch node.
    This implements especially LED status monitoring and sensor calibration and the network uplink
    """
    def __init__(self, config, master, nodeid, nodename):
        super().__init__(config, master, nodeid, nodename)

        # Configure the state machine
        try:
            self.start_command = partial(self.calibrate)
        except KeyError:
            # With no sensor configured, we shall not calibrate
            self.start_command = self.run
        self.run_command = self.run

        self.sensor = WaschSensor(self.nodeid)

        self._device_led_state = -1
        self._expected_led_state = 0

        self.error_state = self.failed

        configtest = [
            config['color_opened'],
            config['color_closed'],
            config['color_pending'],
            config['uplink'],
            ]
        del configtest


    # PLUGIN CALLS
    async def on_msg_status(self, msg):
        """
        When a node sends "STATUS" this is called
        """
        print("manhattan status changed; node: ", self.name, "state: ", msg.result)
        self._expected_led_state = msg.result

        # FIXME: Make me async
        try:
            urllib.request.urlopen(str(self.config['uplink']).format(msg.result))
        except urllib.error.URLError as e:
            print("URLError in MANHATTAN uplink:", e)

    # STATE MACHINE
    async def calibrate(self, channelidx=0):
        """
        Send a sensor_config message to the sensor, filled with the hard-coded values.
        This method has to be prefilled using functools.partial to supply the
        currently selected config.
        """
        msg = None

        if channelidx == manhattan_sensor_channels:
            self.start_command = partial(self.calibrate)
            return self.setup_ssi, None

        msg = self.sensor.configure(
            channelidx,
            *manhattan_sensor_config)

        self.error_state = self.calibration_failed
        return partial(self.calibrate, channelidx + 1), msg

    async def setup_ssi(self):
        """
        Setup the status change blink
        """

        ssi = [(0, 0, self.config['color_pending']),
               (1, 1, self.config['color_pending'])]

        return self.ssi_successful, \
            self.sensor.status_change_indicator(ssi)

    async def enable_sensor(self):
        """
        Enable all configured sensors on the node
        """
        mask = (2 ** manhattan_sensor_channels) - 1
        return self.calibration_successful, \
            self.sensor.enable(str(mask), manhattan_sensor_samplerate)

    async def calibration_failed(self):
        """
        When the calibration timed out, we set it to "unconfigured"
        """
        self.error_state = self.connect
        return self.connect, None

    async def calibration_successful(self):
        """
        When the calibration is successful, just continue to normal run
        """
        self.error_state = self.route
        return self.run, None

    async def ssi_successful(self):
        return self.enable_sensor, None

    async def run(self):
        """
        this is more or less the main loop
        """
        # Default to: do nothing
        next_state = self.run
        msg = None

        if time.time() - self.sent_time > self.config['scandelay']:
            print("Pingtest")
            next_state = self.pingtest
        else:
            #print("Generating led message Should: ",
            #      self._expected_led_state, "is", self._device_led_state)
            msg = await self.generate_led_msg()
            if msg is not None:
                next_state = self.confirm_led_state

        return next_state, msg

    async def failed(self):
        """
        When something has failed - reconnect
        """
        self._device_led_state = []
        self.error_state = self.connect
        return self.connect, None

    async def confirm_led_state(self):
        """
        When the LED state was successfully updated, remember that this was the
        correct last state
        """
        self._device_led_state = self._new_led_state
        self.error_state = self.connect
        return self.run, None

    # HELPER TOOLS
    async def generate_led_msg(self):
        """
        Generate the led message, return None if no update is neccessary
        """
        if self._device_led_state != self._expected_led_state:
            print("\nLED State changed, now {}\n\n".format(self._expected_led_state))
            leds = []
            closed = self.config['color_closed']
            opened = self.config['color_opened']
            if int(self._expected_led_state) == 0:
                leds = [closed, closed]
            elif int(self._expected_led_state) == 1:
                leds = [opened, closed]
            else:
                leds = [opened, opened]

            self._new_led_state = self._expected_led_state
            return self.sensor.led(leds)

def init(master, config):
    """
    Plugin startup
    """
    return None, ManhattanNode
