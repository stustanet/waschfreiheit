#!/usr/bin/env python3

import asyncio
import time
from enum import Enum

class DarkLED:
    OFF = '0'
    GREEN = '1'
    YELLOW = '2'
    RED = '3'
    BLUE = '10'

class MidLED:
    OFF = '0'
    MID_GREEN = '4'
    MID_YELLOW = '5'
    MID_RED = '6'
    BLUE = '11'

class BrightLED:
    OFF = '0'
    GREEN = '7'
    YELLOW = '8'
    RED = '9'
    BLUE = '11'

LED = DarkLED

class Sensor:
    """
    Describes a node within a room.
    Keeps track of the internal state of this node and manages calls to the
    node and callbacks for status
    """
    def __init__(self, master, nodeid, config, loop):
        if loop is None:
            loop = asyncio.get_event_loop()
        self._loop = loop
        self.nodeid = nodeid
        self.master = master
        self.config = config
        self.last_status = ""
        self.last_update = None
        self.state = "CREATED"
        self.distance = config.distance
        self.last_led_command = ""

    async def ping(self, **kwargs):
        """
        Ping the node to test if the path is valid
        """
        await self.master.send_raw("ping {}".format(self.nodeid), self,
                                   expect_response=False, **kwargs)

    async def authping(self, **kwargs):
        """
        Ping a sensor and check if it is still valid and alive
        """
        await self.master.send_raw("authping {}".format(self.nodeid),
                                   self, **kwargs)

    async def connect(self, return_hop, timeout, **kwargs):
        """
        Opens a connection to a hope

        return_hop: temporary first hop on the return path. neccessary to
                open a connection in a not yet fully routed network
        timeout: time to wait in seconds until a package needs to be
                retransmitted
        """
        await self.master.send_raw(
            "connect {} {} {}".format(self.nodeid, return_hop, timeout),
            self, **kwargs)

    async def retransmit(self, **kwargs):
        """
        Retransmits the last packet for the node again
        Call this only after a node sent a timeout.
        """
        await self.master.send_raw("retransmit {}".format(self.nodeid),
                                   self, **kwargs)

    async def configure(self, channel, input_filter, st_matrix,
                        wnd_sizes, reject_filter, **kwargs):

        """
        Set configuration parameters
        """
        await self.master.send_raw("cfg_sensor {} {} {} {} {} {} ".format(
            self.nodeid, channel, input_filter, st_matrix, wnd_sizes,
            reject_filter), self, **kwargs)

    async def enable(self, channel_mask, samples_per_sec, **kwargs):
        """
        Enable/start the sensor
        """
        await self.master.send_raw("enable_sensor {} {} {}"
                                   .format(self.nodeid, channel_mask,
                                           samples_per_sec),
                                   self, **kwargs)

    async def get_rawframes(self, channel, num_frames, **kwargs):
        """
        Get raw frames from a sensor. Used for calibration
        """
        await self.master.send_raw("get_raw {} {} {}".format(
            self.nodeid, channel, num_frames), self, **kwargs)


    async def routes(self, routes, reset=False, **kwargs):
        """
        Set the routes in the network.

        routes in the format : {dst1: hop1, dst2: hop2}
        reset: indicate, if the node should be reset before adding routes
        """
        routestring = ",".join(["{}:{}".format(dst, hop)
                                for dst, hop in routes.items()])
        if reset:
            await self.master.send_raw("reset_routes {} {}"
                                       .format(self.nodeid, routestring),
                                       self, **kwargs)
        else:
            await self.master.send_raw("set_routes {} {}"
                                       .format(self.nodeid, routestring),
                                       self, **kwargs)


    async def resend_state(self, **kwargs):
        """
        Re-send the last led command, in order to recover the state
        """
        if self.last_led_command:
            await self.master.send_raw(self.last_led_command, self, **kwargs)
        else:
            # TODO: change 4 to 5
            await self.led([LED.OFF] * 4)


    async def led(self, ledcolors, **kwargs):
        """
        Set the leds for the sensor
        """
        ledstring = ' '.join(ledcolors)
        self.last_led_command = "led {} {}"\
                                   .format(self.nodeid, ledstring)
        await self.master.send_raw(self.last_led_command, self, **kwargs)


    async def notify_status(self, status):
        """
        An "status" has been received with the given code
        """
        self.master.log.info("Node: %s (%i) status: %s",
                             self.config.name, self.config.id, status)
        self.last_status = status
        self.last_update = time.time()
        for callback in self.config.status_callbacks:
            if asyncio.iscoroutinefunction(callback):
                await callback(self, status)
            else:
                callback(self, status)


    def status_callback(self, cb_func):
        """
        Register a status callback for the given node

        status_callback(statuscode)
        """
        self.config.status_callbacks.append(cb_func)


    def remove_status_callback(self, cb_func):
        """
        Remove the function from the status callbacks
        """
        self.config.status_callbacks = [
            x for x in self.config.status_callbacks
            if x != cb_func]
