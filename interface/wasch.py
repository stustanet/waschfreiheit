#!/usr/bin/env python3

import json
import asyncio
import serial_asyncio
import re
from functools import partial

import sensor

TRANSMITTING = 'transmitting'
CONNECTED = 'connected'
FAILED = 'failed'

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

class WaschOperationInterrupted(WaschError):
    def __init__(self, op = ""):
        self.op = op

    def __repr__(self):
        return "WaschOperationInterrupted: " + self.op

class WaschConfig:
    class Node:
        class Channel:
            def __init__(self, conf={}):
                self.number = conf['number']
                self.name = conf['name']
                self.model = conf['model']
                with open(conf['config']) as f:
                    self.config = json.load(f)

        def __init__(self, conf={}):
            self.id = conf['id']
            self.name = conf['name']
            self.channels = { c['number']: WaschConfig.Node.Channel(c) for c in conf['channels'] }
            self.routes = conf['routes']
            self.samplerate = conf['samplerate']
            self.routes_id = {}
            self.is_initialized = False
            self.distance = -1

    def __init__(self, fname):
        with open(fname) as f:
            conf = json.load(f)

        try:
            self.retransmissionlimit = conf['retransmissionlimit']
            self.networkcheckintervall = conf['networkcheckintervall']
            self.nodes = { c['name']: WaschConfig.Node(c) for c in conf['nodes'] }
            # Postprocess the stuff
            for node in self.nodes.values():
                for rfrom, rto in node.routes.items():

                    node.routes_id[self.nodes[rfrom].id] = self.nodes[rto].id
        except KeyError:
            print("Config failed")
            raise

        self.nodes['MASTER'].distance = 0
        for _ in self.nodes:
            for k,n in self.nodes.items():
                if n.distance >= 0:
                    for m in n.routes.values():
                        if self.nodes[m].distance >= 0:
                            self.nodes[m].distance = min(n.distance + 1,
                                                         self.nodes[m].distance)
                        else:
                            self.nodes[m].distance = n.distance + 1

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

        async def timeout(self, master, node, command):
            command.retransmit_count += 1
            print("Received timeout for command '", command.message,
                  "' count is", command.retransmit_count)
            if command.retransmit_count > self.max_retransmit:
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
            print("Connection made")

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

    class Message:
        def __init__(self, messagestr, node=None):
            self.message = messagestr
            self.retransmit_count = 0
            self.retransmit = False
            self.node = node

        def __repr__(self):
            return self.message

        def command(self):
            if self.retransmit:
                return "retransmit {}\n".format(self.node.nodeid).encode('ascii')

            msg = self.message
            if msg[-1] != '\n':
                msg += '\n'
            return msg.encode('ascii')

    def __init__(self, serial_port, config, baudrate=115200, loop=None,
                 timeoutstrategy=None,
                 network_recovery=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.config = WaschConfig(config)

        if not timeoutstrategy:
            timeoutstrategy = TimeoutStrategy.NRetransmit(self.config.retransmissionlimit)
        self.timeoutstrategy = timeoutstrategy

        self._sensors = {}
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.transport = None
        self._queue = []
        self.last_command = ""
        self.response_pending = False
        self.response_event = asyncio.Event(loop=loop)
        self.message_pending = False
        self.message_event = asyncio.Event(loop=loop)

        if network_recovery == None:
            network_recovery = lambda node: node;
        self.recover_network = network_recovery


        coro = serial_asyncio.create_serial_connection(loop,
                (lambda: WaschInterface.SerialProtocol(self)),
                serial_port, baudrate=baudrate)
        loop.run_until_complete(coro)
        print("Connection setup done")

        self.networkmanager = NetworkManager(self.config, self)

    async def start(self):
        await self.networkmanager.init_the_network()


    async def node(self, nodeid, nexthop=0, timeout=5):
        """
        Register a new sensor node
        """

        confignode = None
        for k, n in self.config.nodes.items():
            if n.id == nodeid:
                confignode = n
                break
        if not confignode:
            raise KeyError("Node with id {} was not configured.".format(nodeid))

        timeout = timeout * confignode.distance
        print("Initing node ID: {}, NAME: {} DiSTANCE: {}".format(
            nodeid, confignode.name, confignode.distance))

        newsensor = sensor.Sensor(self, nodeid, confignode)
        self._sensors[nodeid] = newsensor

        await newsensor.connect(nexthop, timeout)

        return newsensor

    def get_node(self, stringwithnode):
        """
        Get node with the ID
        """
        for n in self._sensors.values():
            if stringwithnode == n.config.name:
                return n

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
            for i, e in self._sensors.items():
                e.status_callback(partial(callback, i))
        else:
            get_node(nodeid).status_callback(partial(callback, nodeid))

    async def send_raw(self, msg, node=None, expect_response=True, force=False, maximum_priority=False):
        """
        Send the raw line to the serial stream

        This will wait, until the preceding message has been processed.
        """
        # The queue is the message stack that has to be processed
        # If the message is a retransmit, it has to be sent (successfully!)
        # before any other command can be executed.
        # Thats why it will be inserted in the beginning of the queue.

        if "retransmit" in msg:
            if not node:
                raise WaschCommandError("You cannot retransmit without node!")
            message = node.last_message
            message.retransmit = True
            self._queue.insert(0, message)
        else:
            message = WaschInterface.Message(msg, node)
            if maximum_priority:
                self._queue.insert(0, message)
            else:
                self._queue.append(message)
            if node:
                node.last_message = message

        while self._queue:
            # Await, if there was another message in the pipeline that has not
            # yet been processed
            if not force:
                while self.message_pending:
                    print("Awaiting response before sending next command")
                    await self.response_event.wait()
                    self.message_event.clear()

            # If the queue has been processed by somebody else in the meantime
            if not self._queue:
                break

            self.response_pending = True
            next_msg = self._queue.pop(0)
            print("Sending", next_msg.command())

            if next_msg.node:
                next_msg.node.state = TRANSMITTING

            # Now actually send the data
            self.last_command = next_msg
            self.transport.write(next_msg.command())

            if not expect_response:
                self.allow_next_message()
            else:
                await self.response_event.wait()
                self.response_event.clear()
                await self.parse_response(self.last_result, self.last_command)
                self.allow_next_message(),

    async def parse_response(self, response, message):
        if response[0:3] == "ERR":
            raise WaschCommandError(message.message)
        elif response[0:3] == "ACK":
            print(response)
            match = re.findall(r"ACK(\d+)-(\d+)", response)
            if not match or len(match) > 1:
                raise WaschCommandError("Invalud ACK result {}".format(response))
            match = match[0]
            self.get_node(match[0]).state = CONNECTED
            code = match[1]
            if code != "128" and code != "0":
                raise WaschAckError(code)
        elif response[0:7] == "TIMEOUT":
            print("Received timeout: ", response)
            node = self.get_node(response[len("TIMEOUT"):])
            try:
                await self.timeoutstrategy.timeout(self, node, message)
            except WaschTimeoutError:
                if self.networkmanager.network_recovery_in_progress:
                    raise
                else:
                    await self.recover_network(node)
        else:
            raise WaschCommandError("Unknown command: {}".format(response))


    def received(self, msg):
        msg = ''.join(msg).strip()
        if not msg:
            return

        #print("received: ", msg)

        match = re.match(r".*###(.*)", msg)
        if match:
            msg, = match.groups(1)

            if str.startswith(msg.upper(), "STATUS"):
                print(msg)
                self.__status(msg[len("STATUS"):])
            else:
                self.last_result = msg
                self.response_pending = False
                self.response_event.set()
        else:
            print("RAW: \"{}\"".format(msg))


    def allow_next_message(self):
        """
        Allow the next message to be sent
        """
        self.message_pending = False
        self.message_event.set()


    def __status(self, msg):
        # The `2:` hack works because the ID is single letter!
        match = re.match(r"(\d+)[ -](\d+)", msg)
        if match:
            node = self.get_node(int(match.groups()[0]))
            node._status(match.groups()[1])
        else:
            raise WaschCommandError("Wasch Interface send invalid status command")


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


class NetworkManager:
    def __init__(self, config, master, loop=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.config = config
        self.master = master
        self.network_recovery_in_progress = False
        self.network_recovery_event = asyncio.Event(loop=loop)
        master.recover_network = self.recover_network


    async def reinit_node(self, node):
        node.is_initialized = False
        await self.init_the_network(node)

    async def init_the_network(self, node=None):
        if not node:
            node = self.config.nodes["MASTER"]

        print("Initing node ", node.name)
        if node.is_initialized:
            return

        if node.name == "MASTER":
            await self.master.sensor_routes(node.routes_id)
            node.is_initialized = True
        else:
            connection = await self.master.node(
                node.id, self.config.nodes[node.routes["MASTER"]].id)
            await connection.routes(node.routes_id, reset=True)
            node.is_initialized = True

        for n in node.routes.values():
            try:
                await self.init_the_network(self.config.nodes[n])
            except WaschOperationInterrupted:
                print("Could not initialize node {}. Please check the hardware."
                      " then your config".format(n))


        if node.name != "MASTER":
            # Send sensor config data,
            bitmask = 0
            for ch in node.channels.values():
                bitmask += 2**int(ch.number)
                await connection.configure(ch.number, *self.parse_sensor_config(ch.config))

            await connection.enable(bitmask, node.samplerate)
            await connection.led(sensor.LED.OFF)


    def parse_sensor_config(self, config):
        #cfg_sensor x 0 20,50,100 0,80,0,-24,0,160,-48,0,160,0,-80,0 150,1500,1500,1500 104,15

        input_filter = ','.join(str(e) for e in [
            config['input_filter']['mid_adjustment_speed'],
            config['input_filter']['lowpass_weight'],
            config['input_filter']['frame_size']])

        transition_matrix = []
        for i in range(0,len(config['transition_matrix'])):
            for j in range(0,len(config['transition_matrix'])):
                if i != j:
                    transition_matrix.append(config['transition_matrix'][i][j])

        transition_matrix = ','.join(str(e) for e in transition_matrix)

        window_sizes = ','.join(str(e) for e in config['window_sizes'])

        reject_filter = ','.join(str (e) for e in [
            config['reject_filter']['threshold'],
            config['reject_filter']['consec_count']])

        return (input_filter, transition_matrix, window_sizes, reject_filter)


    async def recover_network(self, failednode):
        self.network_recovery_failed_node = []
        print("Starting to recover network")
        if self.network_recovery_in_progress:
            print("Another recovery is already in the progress. Waiting for",
                  "it to complete")
            while self.network_recovery_in_progress:
                await self.network_recovery_event.wait()
                self.network_recovery_event.clear()
                if failednode and failednode in self.network_recovery_failed_node:
                    raise WaschOperationInterrupted()
            return
        print("Recovering network...")
        # TODO: Lock the masters queue, to not continue execution
        # maybe use the response_pending logic for that?

        if self.master.response_pending:
            print("Waiting for missing responses on the network")
            await self.master.response_event.wait()

        # Do we have to throw an WaschOperationInterrupted error after we are done?
        throw_error = True

        if not self.master._sensors:
            print("Nodelist is empty at the moment, which means it was not yet",
                  "set up properly.")
            raise WaschOperationInterrupted()
        print("No transmissions are pending, we are happy to proceed with {} nodes"
              .format(len(self.master._sensors)))
        for node in sorted(self.master._sensors.values(), key=lambda n: n.distance):
            print("Checking node ID:{} NAME:{} State:{} ".format(
                node.nodeid, node.config.name, node.state))
            try:
                if node.state == TRANSMITTING:
                    # If the node is currently working on a command (we are in
                    # a retransmit in this case), we will continue sending re-
                    # transmits, and hope for the best.
                    #
                    # If this starts a network recovery, it will raise a timeout
                    # error instead.
                    if node.last_message.retransmit_count > self.master.config.retransmissionlimit:
                        node.state = FAILED
                    else:
                        try:
                            await node.retransmit()
                            if failednode == node:
                                throw_error = False
                        except WaschTimeoutError:
                            node.state = FAILED
                elif node.state == CONNECTED:
                    # If a node is connected, we try to ping it, and if this is
                    # ok, we can relax
                    await node.authping()
                    if failednode == node:
                        throw_error = False
                elif node.state == FAILED:
                    # We ignore failed nodes in recovery.
                    # TODO This has to be improved.
                    continue
                else:
                    await self.reinit_node()
                    if failednode != node:
                        self.network_recovery_failed_node.append(node)
                        self.network_recovery_event.set()
                    else:
                        throw_error = True
            except WaschTimeoutError:
                self.network_recovert_failed_node = node
                await self.reinit_node()

        # TODO: re-enable the masters queue

        self.recovery_in_progress = False
        self.network_recovery_event.set()

        # Only throw an error if we actually run from a failed node
        if failednode and throw_error:
            raise WaschOperationInterrupted()
