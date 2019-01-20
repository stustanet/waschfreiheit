"""
Implementation for a normal waschen node
"""

import math

from basenode import BaseNode
from message import MessageCommand


class WaschNode(BaseNode):
    """
    Implements a waschnode to connect to a waschingmachine node
    This implements especially LED status monitoring and sensor calibration
    """
    def __init__(self, config, master, name, uplink):
        super().__init__(config, master, name)

        self._expected_led_state = []

        self._uplink = uplink

        self._ledmap = None
        try:
            self._ledmap = config['ledmap']
        except KeyError:
            pass

        if self._ledmap is not None:
            # init the state for all leds to 0
            for n in self._ledmap:
                idx = self._ledmap[n]['index']
                if idx >= len(self._expected_led_state):
                    self._expected_led_state += [0] * (1 + idx - len(self._expected_led_state))


        self._channels = []

        try:
            self._channels = config['channels']
        except KeyError:
            pass

        configtest = [
            (c['input_filter']['mid_adjustment_speed'],
             c['input_filter']['lowpass_weight'],
             c['input_filter']['frame_size'],
             c['transition_matrix'],
             c['window_sizes'],
             c['reject_filter']['threshold'],
             c['reject_filter']['consec_count'])
                for c in self._channels]
        del configtest

    def _initialize(self):
        # Insert the state entries for this typeof node
        self._status["CH_INIT"] = 0
        self._status["LED_STATE"] = None

        self._expected_led_state = [0] * len(self._expected_led_state)

    def on_node_status_changed(self, node, status):

        if node == self:
            # Only send updates if the status update was for this node
            self._uplink.on_status_change(node._name, status)

        if self._ledmap is None:
            return

        try:
            entry = self._ledmap[node.name()]
            idx = entry['index']
            color = entry['colors']['s{}'.format(status)]

            self._expected_led_state[idx] = color

        except KeyError:
            pass

    def _on_connected(self, code):
        if not self._status["INITDONE"]:
            # not or no longer initialized -> reset
            self._status["CH_INIT"] = 0
            self._status["LED_STATE"] = None

    def _next_message(self):
        if self._status["CH_INIT"] < len(self._channels):
            # init a channel
            self._status_on_ack = ("CH_INIT", self._status["CH_INIT"] + 1, None)
            return self.__make_calib_message(self._status["CH_INIT"])

        if not self._status["INITDONE"]:
            # The last step of the initialization is to activate the configured sensor channels
            self._status_on_ack = ("INITDONE", True, None)
            return self.__make_enable_sensor_message()

        if self._expected_led_state != self._status["LED_STATE"]:
            # Need to update the LEDs
            self._status_on_ack = ("LED_STATE", self._expected_led_state, None)
            return self.__make_led_message()

        return None

    def __make_calib_message(self, channel):
        # Makes a channel config message for the speified channel

        ch_cfg = self._channels[channel]
        input_filter = ','.join(str(e) for e in [
        ch_cfg['input_filter']['mid_adjustment_speed'],
        ch_cfg['input_filter']['lowpass_weight'],
        ch_cfg['input_filter']['frame_size']])

        # Omit the diagonal (always 0!)
        mx = ch_cfg['transition_matrix']
        transition_matrix = []
        for i in range(int(math.sqrt(len(mx)))):
            for o in range(int(math.sqrt(len(mx)))):
                if i == o:
                    continue
                transition_matrix.append(mx[i * int(math.sqrt(len(mx))) + o])

        transition_matrix = ','.join([str(s) for s in transition_matrix])

        window_sizes = ','.join(str(e) for e in ch_cfg['window_sizes'])

        reject_filter = ','.join(str(e) for e in [
            ch_cfg['reject_filter']['threshold'],
            ch_cfg['reject_filter']['consec_count']])

        return MessageCommand(self._node_id, "cfg_sensor",
                              channel, input_filter, transition_matrix, window_sizes,
                              reject_filter)

    def __make_enable_sensor_message(self):
        """
        Enable all configured sensors on the node
        """
        try:
            mask = self._config['channel_mask']
        except KeyError:
            mask = 2 ** (len(self._config['channels']))-1
        return MessageCommand(self._node_id, "enable_sensor", str(mask), self._config['samplerate'])


    def __make_led_message(self):
        """
        Enable all configured sensors on the node
        """
        ledstring = ' '.join([str(l) for l in self._expected_led_state])
        return MessageCommand(self._node_id, "led", ledstring)
