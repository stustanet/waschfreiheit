#!/usr/bin/env python3

import asyncio
import serial_asyncio
import re

import sensor

class WaschError(Exception):
    pass
class WaschCommandError(WaschError):
    def __init__(self, command):
        self.command = command

    def __repr__(self):
        return "WaschCommandError: {}".format(self.command)

class WaschTimeoutError(WaschError):
    def __init__(self):
        pass
    def __repr__(self):
        return "WaschTimeoutError"

class WaschAckError(WaschError):
    def __init__(self, ackcode):
        self.ackcode = ackcode

    def __repr__(self):
        return "WaschAckError: Ack returned {}".format(self.ackcode)

class TimeoutStrategy:
    """ Different events happening on timeout. """
    class Retransmit:
        """ If a timeout happened - try to retransmit """
        async def timeout(self, master, node, command):
            await node.retransmit()

    class Ignore:
        """ If a timeout happened - just ignore it """
        async def timeout(self, master, node, command):
            pass

    class Exception:
        """
        If a timeout happened - raise an exception and start panic and mayham
        """
        async def timeout(self, master, node, command):
            raise WaschTimeoutError()

    class NRetransmit:
        """
        If a timeout happened - raise an exception and start panic and mayham
        """
        def __init__(self, max_retransmit):
            self.max_retransmit = max_retransmit
            self.last_failed = ""
            self.last_count = 0

        async def timeout(self, master, node, command):
            if command != self.last_failed and not "retransmit" in command:
                self.last_count = 0
                self.last_failed = command

            self.last_count += 1
            print("Received timeout for command", command, "count is", self.last_count)
            if self.last_count > self.max_retransmit:
                raise WaschTimeoutError()

            await node.retransmit()

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


    def __init__(self, serial_port, baudrate=115200, loop=None,
                 timeoutstrategy=TimeoutStrategy.Retransmit()):
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
        self.last_command = ""

        coro = serial_asyncio.create_serial_connection(loop,
                (lambda: WaschInterface.SerialProtocol(self)),
                serial_port, baudrate=baudrate)
        loop.run_until_complete(coro)

    async def node(self, nodeid, nexthop=0, timeout=5):
        """
        Register a new sensor node
        """
        newsensor = sensor.Sensor(self, nodeid)
        self._sensors[nodeid] = newsensor

        await newsensor.connect(nexthop, timeout)

        return newsensor

    def get_node(self, stringwithnode):
        """
        Get node with the ID
        """
        match = re.findall(r'^\d+', str(stringwithnode))
        if match:
            return self._sensors[int(match[0])]
        else:
            raise KeyError()

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

    async def send_raw(self, msg, expect_response=True, force=False):
        """
        Send the raw line to the serial stream

        This will wait, until the preceding message has been processed.
        """
        # The queue is the message stack that has to be processed
        if "retransmit" in msg:
            self._queue.insert(0, msg)
        else:
            self._queue.append(msg)
        while self._queue:
            # Await, if there was another message in the pipeline that has not
            # yet been processed
            if not force:
                while self.response_pending:
                    print("Awaiting response")
                    await self.response_event.wait()
                    self.response_event.clear()
            print("Sending", msg)
            # If the queue has been processed by somebody else in the meantime
            if not self._queue:
                break
            self.response_pending = True
            next_msg = self._queue.pop(0)

            # Now actually send the data
            self.last_command = next_msg
            self.transport.write(next_msg.encode('ascii'))
            #append a newline if the sender hasnt already
            if next_msg[-1] != '\n':
                self.transport.write(b'\n')

            if not expect_response:
                self.response_pending = False
            else:
                await self.response_event.wait()
                self.response_event.clear()
                if self.last_result[0:3] == "ERR":
                    raise WaschCommandError(msg)
                elif self.last_result[0:3] == "ACK":
                    match = re.findall(r"ACK\d*-(\d*)", self.last_result)
                    if not match:
                        raise WaschCommandError("Unknown ACK result {}".format(self.last_result))
                    code = match[0]
                    if code != "128" and code != "0":
                        raise WaschAckError(code)
                elif self.last_result[0:7] == "TIMEOUT":
                    node = self.get_node(self.last_result[7:])
                    await self.timeoutstrategy.timeout(self, node, next_msg)
                else:
                    raise WaschCommandError("Unknown result code: {}".format(self.last_result))


    async def sensor_config(self, node, key_status, key_config):
        """
        node: id of the node to configure
        key_status: common key for the status channel
        key_config: common key for the configuration channel
        both keys have to be 16 bytes long.
        """
        await self.send_raw("config {} {} {}".format(node, key_status, key_config))

    async def sensor_routes(self, routes):
        """
        MASTER ONLY: Set the routes of a sensor node

        Routes in the format : {dst1: hop1, dst2: hop2}
        """
        routestring = ",".join(["{}:{}".format(dst, hop) for dst, hop in routes.items()])
        await self.send_raw("routes {}".format(routestring),
                            expect_response=False)

    def allow_next_message(self):
        """
        Allow the next message to be sent
        """
        self.response_pending = False
        self.response_event.set()

    def received(self, msg):
        msg = ''.join(msg).strip()
        if not msg:
            return
        #print("received", msg)

        match = re.match(r".*###(.*)", msg)
        if match:
            msg, = match.groups(1)

            if str.startswith(msg.lower(), "STATUS"):
                __status(msg[len("STATUS"):])
            else:
                self.last_result = msg
                self.allow_next_message(),
        else:
            print("RAW: \"{}\"".format(msg))

    def __status(self, msg):
        # The `2:` hack works because the ID is single letter!
        self.get_node(msg)._status(msg[2:])
