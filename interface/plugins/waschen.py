"""
Waschen plugin to the waschnetwork
"""

import time
from functools import partial

from node import Node
from message import MessageCommand

class WaschSensor:
    """
    Implements the wrapper between available commands and the commands
    themselves
    """
    def __init__(self, nodeid):
        self.nodeid = nodeid

    def ping(self):
        """
        Ping the node to test if the path is valid
        """
        return MessageCommand(self.nodeid, "ping")

    def authping(self):
        """
        Ping a sensor and check if it is still valid and alive
        """
        return MessageCommand(self.nodeid, "authping")

    def configure(self, channel, input_filter, st_matrix,
                        wnd_sizes, reject_filter):
        """
        Set configuration parameters
        """
        return MessageCommand(self.nodeid, "cfg_sensor",
                              channel, input_filter, st_matrix, wnd_sizes,
                              reject_filter)

    def enable(self, channel_mask, samples_per_sec):
        """
        Enable/start the sensor
        """
        return MessageCommand(self.nodeid, "enable_sensor",
                              channel_mask, samples_per_sec)

    def get_rawframes(self, channel, num_frames, **kwargs):
        """
        Get raw frames from a sensor. Used for calibration
        """
        return MessageCommand(self.nodeid, "get_raw", channel, num_frames)


    def led(self, ledcolors):
        """
        Set the leds for the sensor
        """
        ledstring = ' '.join([str(l) for l in ledcolors])
        return MessageCommand(self.nodeid, "led", ledstring)


    def rebuild_status_channel(self):
        """
        This needs to be called if the reset_routes is skipped after a reconnect
        """
        return MessageCommand(self.nodeid, "rebuild_status_channel")


class WaschNode(Node):
    """
    Implements a waschnode to connect to a waschingmachine node
    This implements especially LED status monitoring and sensor calibration
    """
    def __init__(self, config, master, nodeid, nodename):
        super().__init__(config, master, nodeid, nodename)

        # Configure the state machine
        try:
            self.start_command = partial(self.calibrate, iter(self.config['channels']))
        except KeyError:
            # With no sensor configured, we shall not calibrate
            self.start_command = self.run
        self.run_command = self.run

        self.sensor = WaschSensor(self.nodeid)
        # Configure internal state
        self.is_calibrated = False

        self._device_led_state = []
        self._expected_led_state = []

        self.error_state = self.failed

        configtest = [
            config['ledindex'],
            ]
        del configtest

        try:
            chs = config['channels']
        except KeyError:
            pass
        else:
            configtest = [
                (c['input_filter']['mid_adjustment_speed'],
                 c['input_filter']['lowpass_weight'],
                 c['input_filter']['frame_size'],
                 c['transition_matrix'],
                 c['window_sizes'],
                 c['reject_filter']['threshold'],
                 c['reject_filter']['consec_count'])
                for c in chs]
            del configtest

    # PLUGIN CALLS
    async def on_msg_status(self, msg):
        """
        When a node sends "STATUS" this is called
        """
        print("on_waschen_led_status_change; node: ", self.name, "state: ", msg.result)
        await self.master.pluginmanager.node_call(
            "on_waschen_led_status_change",
            node=self,
            state=msg.result)
        # TODO send stuff to the server
        # The potential update is triggered via the plugin call - below

    async def on_waschen_led_status_change(self, node, state):
        """
        When the LED status of any (as well as this one) has changed, do magic
        """
        idx = node.config['ledindex']
        #import pdb; pdb.set_trace()
        if len(self._expected_led_state) <= idx:
            self._expected_led_state += [0]*(1+idx-len(self._expected_led_state))
        self._expected_led_state[idx] = state

    # STATE MACHINE
    async def calibrate(self, configiterator, channelidx=0):
        """
        Send a sensor_config message to the sensor, filled from configsection
        Thi method has to be prefilled using functools.partial to supply the
        currently selected config.

        It expects config to be an iterator, pointing to the sensor that should
        be currently configured, and will transition into calibration_successful
        once the list has been iterated.
        """
        msg = None
        try:
            config = configiterator.__next__()
        except StopIteration:
            return self.enable_sensor, None

        if not self.is_calibrated:
            input_filter = ','.join(str(e) for e in [
            config['input_filter']['mid_adjustment_speed'],
            config['input_filter']['lowpass_weight'],
            config['input_filter']['frame_size']])

            transition_matrix = ','.join(str(e) for e in config['transition_matrix'])

            window_sizes = ','.join(str(e) for e in config['window_sizes'])

            reject_filter = ','.join(str(e) for e in [
                config['reject_filter']['threshold'],
                config['reject_filter']['consec_count']])
            msg = self.sensor.configure(
                channelidx,
                input_filter,
                transition_matrix,
                window_sizes,
                reject_filter, )

        self.error_state = self.calibration_failed
        return partial(self.calibrate, configiterator, channelidx + 1), msg

    async def enable_sensor(self):
        """
        Enable all configured sensors on the node
        """
        try:
            mask = self.config['channel_mask']
        except KeyError:
            mask = 2 ** (len(self.config['channels']))-1
        return self.calibration_successful, \
            self.sensor.enable(str(mask), self.config['samplerate'])

    async def calibration_failed(self):
        """
        When the calibration timed out, we set it to "unconfigured"
        """
        self.is_calibrated = False
        self.error_state = self.connect
        return self.connect, None

    async def calibration_successful(self):
        """
        When the calibration is successful, just continue to normal run
        """
        self.is_calibrated = True
        self.error_state = self.route
        return self.run, None

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
        self._device_led_state = self._expected_led_state
        self.error_state = self.connect
        return self.run, None

    # HELPER TOOLS
    async def generate_led_msg(self):
        """
        Generate the led message, return None if no update is neccessary
        """
        if self._device_led_state != self._expected_led_state:
            print("\nLED State changed\n\n")
            return self.sensor.led(self._expected_led_state)

def init(master, config):
    """
    Plugin startup
    """
    return None, WaschNode
