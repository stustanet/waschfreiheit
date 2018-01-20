#!/usr/bin/env python3

import time

class LED:
    OFF = '0'
    RED = '9'
    GREEN = '10'
    YELLOW = '11'
    BLUE = '12'

class Sensor:
    def __init__(self, master, nodeid, config):
        self.nodeid = nodeid
        self.master = master
        self.config = config
        self._ack_cbs = []
        self._status_cbs = []
        self.last_status = ""
        self.last_update = None
        self.state = ""
        self.distance = config.distance

    async def ping(self):
        """
        Ping the node to test if the path is valid
        """
        await self.master.send_raw("ping {}".format(self.nodeid), self, expect_response=False)

    async def authping(self):
        """
        Ping a sensor and check if it is still valid and alive
        """
        await self.master.send_raw("authping {}".format(self.nodeid), self)

    async def connect(self, return_hop, timeout):
        """
        Opens a connection to a hope

        return_hop: temporary first hop on the return path. neccessary to
                open a connection in a not yet fully routed network
        timeout: time to wait in seconds until a package needs to be
                retransmitted
        """
        await self.master.send_raw("connect {} {} {}".format(self.nodeid,
                                                             return_hop,
                                                             timeout), self)

    async def retransmit(self):
        """
        Retransmits the last packet for the node again
        Call this only after a node sent a timeout.
        """
        await self.master.send_raw("retransmit {}".format(self.nodeid), self)

    async def configure(self, channel, input_filter, st_matrix,
            wnd_sizes, reject_filter):
        """
        Set configuration parameters
        """
        await self.master.send_raw("cfg_sensor {} {} {} {} {} {} ".format(
            self.nodeid, channel, input_filter, st_matrix, wnd_sizes,
            reject_filter), self)

    async def enable(self, channel_mask, samples_per_sec):
        """
        Enable/start the sensor
        """
        await self.master.send_raw("enable_sensor {} {} {}".format(self.nodeid,
            channel_mask, samples_per_sec), self)

    async def get_rawframes(self, channel, num_frames):
        """
        Get raw frames from a sensor. Used for calibration
        """
        await self.master.send_raw("get_raw {} {} {}".format(self.nodeid, channel,
                                                             num_frames), self)

    async def routes(self, routes, reset=False):
        """
        Set the routes in the network.

        routes in the format : {dst1: hop1, dst2: hop2}
        reset: indicate, if the node should be reset before adding routes
        """
        # TODO Check what does the doku mean with "add_routes"?
        routestring = ",".join(["{}:{}".format(dst, hop) for dst, hop in routes.items()])
        if reset:
            await self.master.send_raw("reset_routes {} {}".format(self.nodeid, routestring), self)
        else:
            await self.master.send_raw("set_routes {} {}".format(self.nodeid, routestring), self)


    async def led(self, ledcolors):
        if type(ledcolors) not in (list, tuple):
            ledcolors = [str(ledcolors)] * 5
        ledstring = ' '.join(ledcolors)
        await self.master.send_raw("led {} {}        ".format(self.nodeid, ledstring))

    def _ack(self, code):
        """
        An "ack" has been received with the given code
        """
        for x in self._ack_cbs:
            x(code)

    def _status(self, status):
        """
        An "status" has been received with the given code
        """
        self.last_status = status
        self.last_update = time.time()
        for x in self._status_cbs:
            x(status)

    def ack_callback(self, ack_func):
        """
        Register a ack-callback for the given node

        ack_callback(statuscode)
        """
        self._ack_cbs.append(ack_func)

    def remove_ack_callback(self, ack_callback):
        self._ack_cbs = [ x for x in self._ack_cbs if x != ack_callback ]

    def status_callback(self, cb_func):
        """
        Register a status callback for the given node

        status_callback(statuscode)
        """
        self._status_cbs.append(cb_func)

    def remove_status_callback(self, cb_func):
        self._status_cbs = [ x for x in self._status_cbs if x != cb_func ]
