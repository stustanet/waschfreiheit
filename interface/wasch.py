#!/usr/bin/env python3

import asyncio
import serial_asyncio
import re

import sensor

class WaschException(Exception):
    pass

class TimeoutStrategy:
    """ Different events happening on timeout. """
    class Retransmit:
        """ If a timeout happened - try to retransmit """
        def timeout(self, master, node):
            node.retransmit()

    class Ignore:
        """ If a timeout happened - just ignore it """
        def timeout(self, master, node):
            pass

    class Exception:
        """
        If a timeout happened - raise an exception and start panic and mayham
        """
        def timeout(self, master, node):
            raise WaschException()

class WaschInterface:
    """
    The Waschinterface is the controller of the master node.
    It has to be attached via serial bus to the master node of the
    wascheninfreiheit master node.

    It provides all functionality available as function calls and will handle
    the responses of the network.

    To communicate with a node, you have to start it with .node(NODEID).
    Now you can issue commands to the node using asyncio.

    AS a setup you need to initialise the master
    ```
    import wasch
    master = wasch.WaschInterface("/dev/ttyUSB0", loop=loop)
    ```

    Then you register your node
    ```
    node = master.node(1)
    ```

    Now you can issue comands to this node.
    ```
    loop.run_until_complete(node.ping())
    loop.run_until_complete(node.authping())
    ```

    In order to receive status updates, a status callback will be called.
    For that, call .status_callback(callback) with an optional nodeid parameter

    The nodes themselves have a node.last_status field, which contains the
    last updated state
    """

    class SerialProtocol(asyncio.Protocol):
        def __init__(self, master):
            self.master = master
            self.buffer = []

        def connection_made(self, transport):
            #transport.serial.rts = False
            self.master.transport = transport

        def data_received(self, data):
            for d in data:
                if chr(d) == '\n':
                    self.master.received(self.buffer)
                    self.buffer = []
                else:
                    self.buffer.append(chr(d))

        def connection_lost(self, exc):
            print('port closed, exiting')
            asyncio.get_event_loop().stop()


    def __init__(self, serial_port, baudrate=9600, loop=None,
            timeoutstrategy=TimeoutStrategy.Retransmit):
        if loop == None:
            loop = asyncio.get_event_loop()
        self._sensors = {}
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.timeoutstrategy = timeoutstrategy
        self.transport = None
        self.response_pending = False
        self._queue = []
        self.response_event = asyncio.Event(loop=loop)

        coro = serial_asyncio.create_serial_connection(loop,
                (lambda: WaschInterface.SerialProtocol(self)),
                serial_port, baudrate=baudrate)
        loop.run_until_complete(coro)

    def node(self, nodeid):
        """
        Register a new sensor node
        """
        newsensor = sensor.Sensor(self, nodeid)
        self._sensors[nodeid] = newsensor
        return newsensor

    def get_node(self, stringwithnode):
        """
        Get node with the ID
        """
        match = re.findall(r'^\d+', str(stringwithnode))

        return self._sensors[int(match[0])]

    def status_subscribe(self, callback, nodeid=None):
        """
        Subscribe to status updates from the node network

        callback(nodeid, state)

        if nodeid is None, subscribe to all sensors
        """

        if nodeid == None:
            for e in self._sensors.values():
                e.status_callback(lambda s: callback(e.nodeid, s))
        else:
            get_node(nodeid).status_callback(lambda s: callback(nodeid, s))

    async def sensor_routes(self, routes):
        """
        DEBUG ONLY: Set the routes of a sensor node

        Routes in the format : {dst1: hop1, dst2: hop2}
        """
        routestring = ",".join(["{}:{}".format(dst, hop) for dst, hop in routes.items()])
        await self.send_raw("routes {}".format(routestring))

    async def sensor_config(self, node, key_status, key_config):
        """
        node: id of the node to configure
        key_status: common key for the status channel
        key_config: common key for the configuration channel
        both keys have to be 16 bytes long.
        """
        await self.send_raw("config {} {} {}".format(node, key_status, key_config))

    async def routes(self, routes, reset=False):
        """
        Set the routes in the network.

        routes in the format : {dst1: hop1, dst2: hop2}
        reset: indicate, if the node should be reset before adding routes
        """
        # TODO Check what does the doku mean with "add_routes"?
        routestring = ",".join(["{}:{}".format(dst, hop) for dst, hop in routes.items()])
        if reset:
            self.send_raw("reset_routes {}".format(routestring))
        else:
            self.send_raw("set_routes {}".format(routestring))

    async def send_raw(self, msg, expect_response=True):
        """
        Send the raw line to the serial stream

        This will wait, until the preceding message has been processed.
        """
        # The queue is the message stack that has to be processed
        self._queue.append(msg)
        while self._queue:
            # Await, if there was another message in the pipeline that has not
            # yet been processed
            while self.response_pending:
                await self.response_event.wait()
                self.response_event.clear()
            # If the queue has been processed by somebody else in the meantime
            if not self._queue:
                break
            self.response_pending = True
            next_msg = self._queue.pop(0)

            # Now actually send the data
            self.transport.write(next_msg.encode('ascii'))
            #append a newline if the sender hasnt already
            if next_msg[-1] != '\n':
                self.transport.write(b'\n')

            if not expect_response:
                self.response_pending = False

    def allow_next_message(self):
        """
        Allow the next message to be sent
        """
        self.response_pending = False
        self.response_event.set()

    def err(self):
        raise WaschException()

    def received(self, msg):
        msg = ''.join(msg).strip()
        if not msg:
            return

        if str.startswith(msg, "###"):
            msg = msg[3:].strip()
            commands = {
                    # The `2:` hack works because the ID is single letter!
                    "ACK": (lambda s:
                        self.get_node(msg[3:])._ack(msg[5:])),
                    "ERR": (lambda s:
                        self.err()),
                    "TIMEOUT": (lambda s:
                        self.timeoutstrategy.timeout(self, self.get_node(msg[7:]))),
                    # The `2:` hack works because the ID is single letter!
                    "STATUS": (lambda s: self.get_node(msg[6:])._status(msg[8:])),
            }

            has_run = False
            for c in commands:
                if str.startswith(msg.lower(), c.lower()):
                    try:
                        commands[c](msg[len(c):])
                    except:
                        raise
                    finally:
                        if c != "STATUS":
                            # Trigger the sending system to send the next
                            # message
                            self.allow_next_message(),
                    has_run = True
                    break

            if not has_run:
                print("Unknown command ", msg)
        else:
            print("Unknown instruction received \"{}\"".format(msg))
