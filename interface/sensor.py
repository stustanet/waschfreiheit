#!/usr/bin/env python3

import time

class Sensor:
    def __init__(self, master, nodeid):
        self.nodeid = nodeid
        self.master = master
        self._ack_cbs = []
        self._status_cbs = []
        self.last_status = ""
        self.last_update = None

    async def ping(self):
        """
        Ping the node to test if the path is valid
        """
        await self.master.send_raw("ping {}".format(self.nodeid))

    async def authping(self):
        """
        Ping a sensor and check if it is still valid and alive
        """
        await self.master.send_raw("authping {}".format(self.nodeid))

    async def connect(self, return_hop, timeout):
        """
        Opens a connection to a hope

        return_hop: temporary first hop on the return path. neccessary to
                open a connection in a not yet fully routed network
        timeout: time to wait in seconds until a package needs to be
                retransmitted
        """
        await self.master.send_raw("connect {} {} {}".format(self.nodeid, return_hop,
            timeout))

    async def retransmit(self):
        """
        Retransmits the last packet for the node again
        Call this only after a node sent a timeout.
        """
        await self.master.send_raw("retransmit {}".format(self.nodeid))

    async def configure(self, channel, input_filter, st_matrix,
            wnd_sizes, reject_filter):
        """
        Set configuration parameters
        """
        await self.master.send_raw("configure_sensor {} {} {} {} {} {} ".format(
            self.nodeid, channel, input_filter, st_matrix, wnd_sizes,
            reject_filter))

    async def enable(self, channel_mask, samples_per_sec):
        """
        Enable/start the sensor
        """
        await self.master.send_raw("enable_sensor {} {} {}".format(self.nodeid,
            channel_mask, samples_per_sec))

    async def get_rawframes(self, channel, num_frames):
        """
        Get raw frames from a sensor. Used for calibration
        """
        await self.master.send_raw("get_raw {} {} {}", self.nodeid, channel,
                num_frames)

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
