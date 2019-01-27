"""
Implementation for the open switch for the Manhattan bar
"""

from basenode import BaseNode
from message import MessageCommand

# There is no reason why someone would like to have other params than this for this type of sensor,
# so we just hard-code this values here
manhattan_sensor_config = ("0,0,1", "0,16500,0,0,0,0,-16000,0,0,0,0,0", "3,3,3,3", "1,1")
manhattan_sensor_channels = 2
manhattan_sensor_samplerate = 500


class ManhattanNode(BaseNode):
    def __init__(self, config, master, name, uplink):
        super().__init__(config, master, name)

        # Insert the state entries for this typeof node
        self._uplink = uplink

        configtest = [
            config['color_opened'],
            config['color_closed'],
            config['color_pending'],
            config['uplink'],
            config['uplink_key'],
            ]

    def _initialize(self):
        self._status["CH_INIT"] = 0
        self._status["CSSI"] = False
        self._status["LED_STATE"] = None

        self._expected_led_state = [0, 0]


    def on_node_status_changed(self, node, status):

        if node is not self:
            # This node does not care about other nodes at all
            return

        # Not actually needed, I just do this so it shows up in the state dump
        self._status["LAST_STATUS"] = status

        url = '{}?key={}&status={}'.format(self._config['uplink'], self._config['uplink_key'], status)
        self._uplink.raw_request(url)

        r = self._config['color_closed']
        g = self._config['color_opened']

        if int(status) == 1:
            self._expected_led_state = [g, r]
        elif int(status) == 2:
            self._expected_led_state = [g, g]
        else:
            self._expected_led_state = [r, r]

    def _on_connected(self, code):
        if not self._status["INITDONE"]:
            # not or no longer initialized -> reset
            self._status["CH_INIT"] = 0
            self._status["CSSI"] = False
            self._status["LED_STATE"] = None

    def _next_message(self):
        if self._status["CH_INIT"] < manhattan_sensor_channels:
            # init a channel
            self._status_on_ack = ("CH_INIT", self._status["CH_INIT"] + 1, None)
            return MessageCommand(self._node_id, "cfg_sensor", self._status["CH_INIT"], *manhattan_sensor_config)

        if not self._status["CSSI"]:
            # Configure the blnking LEDs on status change
            self._status_on_ack = ("CSSI", True, None)
            return self.__make_cssi_message()

        if not self._status["INITDONE"]:
            # The last step of the initialization is to activate the configured sensor channels
            self._status_on_ack = ("INITDONE", True, None)
            return MessageCommand(self._node_id, "enable_sensor", 3, manhattan_sensor_samplerate)

        if self._expected_led_state != self._status["LED_STATE"]:
            # Need to update the LEDs
            self._status_on_ack = ("LED_STATE", self._expected_led_state, None)
            return self.__make_led_message()

        return None


    def __make_cssi_message(self):
        ssistr = "0,0,{} 1,1,{}".format(self._config['color_pending'], self._config['color_pending'])
        return MessageCommand(self._node_id, "cfg_status_change_indicator", ssistr)


    def __make_led_message(self):
        """
        Enable all configured sensors on the node
        """
        ledstring = ' '.join([str(l) for l in self._expected_led_state])
        return MessageCommand(self._node_id, "led", ledstring)
