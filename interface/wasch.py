#!/usr/bin/env python3

import json
import asyncio
import re
import logging
from serial import SerialException
import serial_asyncio

import sensor

TRANSMITTING = 'transmitting'
CONNECTED = 'connected'
FAILED = 'failed'


class WaschError(Exception):
    """
    Base Wasch Exception, can be used to catch all wasch related errors
    """
    pass


class WaschCommandError(WaschError):
    """
    A Command has returned as an error from the node. Check your parameters, yo
    """
    def __init__(self, command):
        super().__init__()
        self.command = command

    def __repr__(self):
        super()
        return "WaschCommandError: {}".format(self.command)


class WaschTimeoutError(WaschError):
    """
    A timeout has occurred within the ocmmunication with the machine
    This is normal! Try to issue a retransmit to the node.
    If this happens for a longer while, you should think about what to do
    """
    def __init__(self):
        super().__init__()

    def __repr__(self):
        return "WaschTimeoutError"


class WaschAckError(WaschError):
    """
    Something has not been ACK-ed correctly. This is strange.
    We should investigate!
    """
    def __init__(self, ackcode):
        super().__init__()
        self.ackcode = ackcode

    def __repr__(self):
        return "WaschAckError: Ack returned {}".format(self.ackcode)


class WaschOperationInterrupted(WaschError):
    """
    The last queued operation has been interrupted. Do not expect the node
    to even know you anymore, it might have encountered full amnesia together
    with its basic programming
    """
    def __init__(self, op=""):
        super().__init__()
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
            self.status_callbacks = []
            self.id = conf['id']
            self.name = conf['name']
            self.channels = {c['number']: WaschConfig.Node.Channel(c)
                             for c in conf['channels']}
            self.routes = conf['routes']
            self.samplerate = conf['samplerate']
            self.routes_id = {}
            self.is_initialized = False
            self.distance = -1

    def __init__(self, fname, log=None):
        with open(fname) as configfile:
            conf = json.load(configfile)

        try:
            self.retransmissionlimit = conf['retransmissionlimit']
            self.networkcheckintervall = conf['networkcheckintervall']
            self.single_hop_timeout = conf['single_hop_timeout']
            self.nodes = {c['name']: WaschConfig.Node(c)
                          for c in conf['nodes']}
            # Postprocess the stuff
            for node in self.nodes.values():
                for rfrom, rto in node.routes.items():

                    node.routes_id[self.nodes[rfrom].id] = self.nodes[rto].id
        except KeyError as exp:
            log.error("Network config setup failed")
            log.exception(exp)
            raise

        self.nodes['MASTER'].distance = 0
        for _ in self.nodes:
            for k, n in self.nodes.items():
                if n.distance < 0:
                    continue

                for m in n.routes.values():
                    if self.nodes[m].distance >= 0:
                        self.nodes[m].distance = min(n.distance + 1,
                                                     self.nodes[m].distance)
                    else:
                        self.nodes[m].distance = n.distance + 1


class TimeoutStrategy:
    """ Different events happening on timeout. """
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
            """
            to be called (maybe recursively!!) when a timeout happened
            """
            command.retransmit_count += 1
            master.log.info("Received timeout for command %s. count is %i",
                            command.message, command.retransmit_count)
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

    class Message:
        """
        Message object that is currently transferred to the node.
        This is used to keep track of the number of retransmits and to make
        shure the messages are well-formed
        """
        def __init__(self, messagestr, node=None):
            self.message = messagestr
            self.retransmit_count = 0
            self.retransmit = False
            self.node = node

        def __repr__(self):
            return self.message

        def command(self):
            """
            Representation of the command so that it can be issued on the line
            directly
            """
            if self.retransmit:
                return "retransmit {}\n".format(self.node.nodeid)\
                                        .encode('ascii')

            msg = self.message
            if msg[-1] != '\n':
                msg += '\n'
            return msg.encode('ascii')

    def __init__(self, serial_port, configfile, baudrate=115200, loop=None):
        # Base setup
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop

        # Setup the log
        logformat = '%(asctime)s | %(name)s | %(levelname)s | %(message)s'
        logging.basicConfig(format=logformat)
        self.log = logging.getLogger('waschmaster')
        self.log.setLevel(logging.DEBUG)
        self.log.info("starting wasch interface")

        # the configuration to use
        self.config = WaschConfig(configfile, log=self.log)
        # the serial port to use
        self.serial_port = serial_port
        # with the corresponding baud-rate
        self.baudrate = baudrate

        # message synchronisation to avoid multiple commands on the line at the
        # same time
        self.message_pending = False
        self.message_event = asyncio.Event(loop=loop)
        self.running_message = None
        # the list of sensors that are currently known and have at least once
        # connected successfully
        self.sensors = {}

        # the queue of messages pending
        self._queue = []

        self.timeoutstrategy = TimeoutStrategy.NRetransmit(
            self.config.retransmissionlimit)

        self.log.info("opening serial connection to %s", serial_port)
        # set up the serial connection
        coro = serial_asyncio.open_serial_connection(
            url=serial_port,
            baudrate=baudrate, loop=loop)
        self.all_reader, self.writer = loop.run_until_complete(coro)

        # the reader will be fed by the connection observer, that will always
        # listen and not miss a ###status update.
        self.reader = asyncio.StreamReader()
        self.statusqueue = asyncio.StreamReader()

        self._network_task = self.loop.create_task(
            self._network_filter_task())

        # It will be started after the network setup is done.
        self._status_task = None

        self.networkmanager = NetworkManager(self.config, self)
        self.log.info("connection setup done")

    async def _network_filter_task(self):
        """
        The purpose of this task is to listen for network events, and redirect
        them to the correct notification queues

        It has to be separated into multiple streams, else we would miss
        responses to messages that are sent in the status processing queues
        """
        while True:
            try:
                line = await self.all_reader.readline()
                self.log.debug("RECV %s", str(line))
                # check, if this was a status message
                if str.startswith(line.decode('ascii').upper(),
                                  "###STATUS"):
                    #self.log.debug("Redirecting to STATUS queue")
                    self.statusqueue.feed_data(line)
                else:
                    #self.log.debug("Redirecting to MAIN queue")
                    self.reader.feed_data(line)
            except SerialException:
                self.log.error("Lost contact to our node. This is bad!")
                self.loop.stop()
                break

    async def _status_update_task(self):
        self.log.info("Started STATUS watcher")
        while True:
            try:
                line = await self.statusqueue.readline()
                line = line.decode('ascii')
                self.log.info("Processing status message %s", line)
                await self.__status(line[len("###STATUS"):])
            except asyncio.CancelledError:
                raise
            except Exception as e:
                self.log.error("Exception during status processing")
                self.log.exception(e)
                raise

    async def start(self):
        """
        Start the networking process
        """
        await self.networkmanager.init_the_network()
        self._status_task = self.loop.create_task(
            self._status_update_task())


    async def teardown(self):
        """
        Clean up the async code
        """
        try:
            self._network_task.cancel()
            await self._network_task
        except asyncio.CancelledError:
            pass
        try:
            if self._status_task:
                self._status_task.cancel()
                await self._status_task
        except asyncio.CancelledError:
            pass

    async def node(self, nodeid, nexthop=0, timeout=None):
        """
        Register a new sensor node
        """
        confignode = None
        for node in self.config.nodes.values():
            if node.id == nodeid:
                confignode = node
                break
        if not confignode:
            raise WaschError("Node with id {} was not specified in the config "
                             "file.".format(nodeid))

        if timeout is None:
            timeout = self.config.single_hop_timeout

        timeout = timeout * confignode.distance
        self.log.info("Initing node ID: %i, NAME: %s DiSTANCE: %i",
                      nodeid, confignode.name, confignode.distance)

        # If we insert the new sensor here, it is be added to the recovery in
        # in case it is not successfull to connect
        newsensor = sensor.Sensor(self, nodeid, confignode, loop=self.loop)
        self.sensors[nodeid] = newsensor
        await newsensor.connect(nexthop, timeout)

        return newsensor

    def get_node(self, stringwithnode):
        """
        Get node with the ID. This tries to be clever.
        """
        for node in self.sensors.values():
            if stringwithnode == node.config.name:
                return node

        match = re.findall(r'^\d+', str(stringwithnode))
        if match:
            return self.sensors[int(match[0])]
        else:
            raise KeyError("Could not find Node with id {}"
                           .format(stringwithnode))

    def status_subscribe(self, callback, nodeid=None):
        """
        Subscribe to status updates from the node network

        callback(node, state)

        if nodeid is None, subscribe to all sensors
        """
        if nodeid is None:
            for node in self.config.nodes.values():
                node.status_callbacks.append(callback)
        else:
            self.get_node(nodeid).status_callback(callback)

    async def __status(self, msg):
        self.log.info("Received status update: %s", msg)
        match = re.match(r"(\d+)[ -](\d+)", msg)
        if match:
            nodeid, status = match.groups()
            self.log.debug("Updating status of node %i to %s",
                           int(nodeid), status)
            node = self.get_node(int(nodeid))
            await node.notify_status(status)
        else:
            raise WaschCommandError("Wasch Interface send invalid status "
                                    + "command")

    def clear_queue_from_node(self, node):

        self._queue = [ q for q in self._queue if q.node != node ]

        if self.running_message.node == node:
            self.running_message = None
            self.message_pending = False
            self.message_event.set()

    async def send_raw(self, msg, node=None, expect_response=True):
        """
        Send the raw line to the serial stream

        This will wait, until the preceding message has been processed.
        """
        # The queue is the message stack that has to be processed
        # If the message is a retransmit, it has to be sent (successfully!)
        # before any other command can be executed.
        # That's why it will be inserted in the beginning of the queue.
        if "retransmit" in msg:
            if not node:
                raise WaschCommandError("You cannot retransmit without node!")
            message = node.last_message
            message.retransmit = True
            self._queue.insert(0, message)
        else:
            message = WaschInterface.Message(msg, node)
            self._queue.append(message)
            if node:
                node.last_message = message

        while self._queue:
            # wait, if there was another message in the pipeline that has not
            # yet been processed. We need to lock across these messages to
            # avoid sending multiple messages to the master node.
            while self.message_pending and not self.running_message.retransmit:
                self.log.info("Awaiting response to %s before sending %s",
                              self.running_message, msg)
                await self.message_event.wait()
                self.message_event.clear()

            # If the queue has been processed by somebody else in the meantime
            if not self._queue:
                break

            self.message_pending = True
            next_msg = self._queue.pop(0)
            self.running_message = next_msg
            self.log.debug("SEND %s", next_msg.command())

            if next_msg.node:
                next_msg.node.state = TRANSMITTING

            # Now actually send the data
            self.writer.write(next_msg.command())

            if not expect_response:
                self.message_pending = False
                self.message_event.set()
            else:
                while True:
                    #print("Reader: ", asyncio.Task.current_task())
                    #print("Waiting for ", self.running_message)
                    try:
                        line = await self.reader.readline()
                    except asyncio.TimeoutError:
                        self.networkmanager.recover_network(next_msg.node)

                    #print("DONE: ", asyncio.Task.current_task())
                    # parse response will return true, if we can continue
                    # sending data
                    if await self.parse_response(line, next_msg):
                        #print("ACKed message", self.running_message)
                        break
                self.message_pending = False
                self.message_event.set()


    async def parse_response(self, response, message):
        """
        Parse a response and extract the relevant information
        """
        try:
            response = response.decode('ascii')
        except:
            pass

        idx = response.find('###')
        if idx > 0:
            response = response[idx:]

        if str.startswith(response.upper(), "###ERR"):
            await self.networkmanager.recover_network(None)
        elif str.startswith(response.upper(), "###ACK"):
            match = re.findall(r"###ACK(\d+)-(\d+)", response)
            if not match or len(match) > 1:
                raise WaschCommandError("Invalid ACK result {}"
                                        .format(response))
            match = match[0]
            self.get_node(match[0]).state = CONNECTED
            code = match[1]
            if code != "128" and code != "0":
                raise WaschAckError(code)
            return True
        elif str.startswith(response.upper(), "###TIMEOUT"):
            node = self.get_node(response[len("###TIMEOUT"):])
            self.log.info("Timeout for node %s (%i).",
                          node.config.name, node.nodeid)
            try:
                await self.timeoutstrategy.timeout(self, node, message)
                return True
            except WaschTimeoutError:
                if self.networkmanager.network_recovery_in_progress:
                    self.log.info("Recovery already in progress. Handing "
                                  "over to error handling")
                    raise
                else:
                    self.log.warning("Starting network recovery after timeout")
                    await self.networkmanager.recover_network(node)
                    return False
        elif str.startswith(response.upper(), "###"):
            raise WaschCommandError("Unknown command: {}".format(response))
        return False

    def received(self, msg):
        msg = ''.join(msg).strip()
        if not msg:
            return

        print("received: ", msg)

        match = re.match(r".*###(.*)", msg)
        if match:
            msg, = match.groups(1)

            if str.startswith(msg.upper(), "STATUS"):
                print(msg)
                __status(msg[len("STATUS"):])
            else:
                self.last_result = msg
                self.response_pending = False
                self.response_event.set()
                self.allow_next_message(),
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
        self.get_node(msg)._status(msg[2:])

    async def sensor_config(self, node, key_status, key_config):
        """
        node: id of the node to configure
        key_status: common key for the status channel
        key_config: common key for the configuration channel
        both keys have to be 16 bytes long.
        """
        await self.send_raw("config {} {} {}"
                            .format(node, key_status, key_config))


    async def sensor_routes(self, routes):
        """
        MASTER ONLY: Set the routes of a sensor node

        Routes in the format : {dst1: hop1, dst2: hop2}
        """
        routestring = ",".join(["{}:{}".format(dst, hop)
                                for dst, hop in routes.items()])
        await self.send_raw("routes {}".format(routestring),
                            expect_response=False)

class NetworkManager:
    """
    The Networkmanager is responsible for network setup and management.

    It converts the configfile to the nodes to initialize and holds the
    protocols to scan and recover the network
    """

    def __init__(self, config, master, loop=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.config = config
        self.master = master
        self.network_recovery_in_progress = False
        self.network_recovery_event = asyncio.Event(loop=loop)
        self.network_recovery_failed_node = []
        self.recovering_task = None


    async def reinit_node(self, node):
        """
        Initialize a node, that has previously been marked as valid and working

        This is required to do whenever we assume, that a node is dead
        """
        snsor = None
        if isinstance(node, sensor.Sensor):
            snsor = node
            self.master.clear_queue_from_node(snsor)
            node = node.config
        node.is_initialized = False
        await self.init_the_network(node)

        if snsor:
            try:
                await snsor.resend_state()
            except WaschError as exp:
                self.master.log.exception(exp)

    async def init_the_network(self, node=None):
        """
        Perform a network setup by initializing all uninitialized nodes

        This is performed by recursively walking through the graph, starting
        from the master node and sending "CONNECT" and "ROUTES".
        As soon as the network has been configured, "CONFIGURE", "ENABLE"
        are issued in reverse order to start up the network from the remotest
        node first

        The recursion is performed according to the routes defined in the
        config file. If a node does not have a valid node, it will not be
        initialized.

        !! IMPORTANT: The config file _NEEDS_ a MASTER node!
        """
        if not node:
            node = self.config.nodes["MASTER"]

        if node.is_initialized:
            return

        try:
            if self.master.get_node(node.name).state == FAILED:
                return
        except KeyError:
            # The node was not yet initialized, this is fine
            pass


        self.master.log.info("Initializing node %s", node.name)

        if node.name == "MASTER":
            await self.master.sensor_routes(node.routes_id)
            node.is_initialized = True
        else:
            connection = await self.master.node(
                node.id, self.config.nodes[node.routes["MASTER"]].id)
            await connection.routes(node.routes_id, reset=True)
            node.is_initialized = True

        for nodename in node.routes.values():
            try:
                await self.init_the_network(self.config.nodes[nodename])
            except WaschOperationInterrupted:
                self.master.log.error(
                    "Could not initialize node %s. Check the hardware, "
                    "then your config", nodename)

        if node.name == "MASTER":
            return

        # Send sensor config data,
        bitmask = 0
        for channel in node.channels.values():
            bitmask += 2**int(channel.number)
            await connection.configure(
                channel.number,
                *self.parse_sensor_config(channel.config))

        await connection.enable(bitmask, node.samplerate)
        await connection.led([sensor.LED.OFF] * 5)

    def parse_sensor_config(self, config):
        """
        Read the sensor specific config file and convert it into a tupel
        that can be sent to the node
        """
        input_filter = ','.join(str(e) for e in [
            config['input_filter']['mid_adjustment_speed'],
            config['input_filter']['lowpass_weight'],
            config['input_filter']['frame_size']])

        transition_matrix = []
        for i in range(0, len(config['transition_matrix'])):
            for j in range(0, len(config['transition_matrix'])):
                if i != j:
                    transition_matrix.append(config['transition_matrix'][i][j])

        transition_matrix = ','.join(str(e) for e in transition_matrix)

        window_sizes = ','.join(str(e) for e in config['window_sizes'])

        reject_filter = ','.join(str(e) for e in [
            config['reject_filter']['threshold'],
            config['reject_filter']['consec_count']])

        return (input_filter, transition_matrix, window_sizes, reject_filter)

    async def recover_network(self, failednode):
        """
        Perform a network recovery

        Perform the following pseudo codes

        ```
        for all nodes ordered by their distance:
            if ping() successfull
                continue
            else
                connect()
`       ```

        That way this function can be used to scan the network for proper
        operation as well as the recovery if a node has failed
        """

        if asyncio.Task.current_task:
            if self.recovering_task == asyncio.Task.current_task:
                return False

        if self.network_recovery_in_progress:
            self.master.log.info("Another recovery is already in the progress "
                                 "Waiting for it to complete")
            while self.network_recovery_in_progress:
                await self.network_recovery_event.wait()
                self.network_recovery_event.clear()
                if failednode in self.network_recovery_failed_node:
                    raise WaschOperationInterrupted()
            return
        self.recovering_task = asyncio.Task.current_task
        self.network_recovery_in_progress = True
        self.network_recovery_failed_node = []
        self.master.log.info("Scanning Network for proper operation")

        if self.master.message_pending:
            timeout = self.master.running_message.node.distance \
                      * self.config.single_hop_timeout
            self.master.log.info("Waiting for missing responses on the "
                                 "network for max %s seconds", timeout)
            try:
                await asyncio.wait_for(
                    self.master.message_event.wait(),
                    timeout)
            except asyncio.TimeoutError:
                self.master.log.error("Needed to kill a running transaction")

        # Do we have to raise WaschOperationInterrupted error after we're done?
        throw_error = True
        if not self.master.sensors:
            self.master.log.error("Nodelist is empty at the moment, which "
                                  "means it was not yet set up properly.")
            raise WaschOperationInterrupted()
        self.master.log.info("No transmissions are pending, we are happy to "
                             "proceed with %s nodes", len(self.master.sensors))
        for node in sorted(self.master.sensors.values(),
                           key=lambda n: n.distance):
            self.master.log.info("Checking node %s (%s) State: %s ",
                                 node.config.name, node.nodeid, node.state)
            try:
                timeout = node.distance * 5
                if node.state == TRANSMITTING:
                    # If the node is currently working on a command (we are in
                    # a retransmit in this case), we will continue sending re-
                    # transmits, and hope for the best.
                    #
                    # If this tries to start a network recovery, it will raise
                    # a timeout error instead.
                    try:
                        self.master.log.info("Waiting for the node to finish "
                                             "for %s seconds", timeout)
                        await asyncio.wait_for(self.finish_retransmit(node),
                                               timeout)
                        self.master.log.info("Retransmit successfull")
                        # If this completed successfully, we do not want to die
                        if failednode == node:
                            throw_error = False
                    except (WaschTimeoutError, asyncio.TimeoutError):
                        self.master.log.info("Node has timeouted")
                        node.state = FAILED
                elif node.state == CONNECTED:
                    # If a node is connected, we try to ping it, and if this is
                    # ok, we can relax
                    self.master.log.info("Pinging node %s", node.config.name)
                    await node.authping()
                    self.master.log.info("ping successfull")
                    if failednode == node:
                        throw_error = False
                elif node.state == FAILED:
                    # We ignore failed nodes in recovery.
                    # TODO This has to be improved.
                    self.master.log.info("Node %s (%i)is in failed state",
                                         node.config.name, node.nodeid )
                    await self.reinit_node(node)
                else:
                    self.master.log.info("FALLBACK: Node %s has state: %s",
                                         node.name, node.state)
                    await self.reinit_node(node)
                    update_led = True
                    if failednode != node:
                        self.network_recovery_failed_node.append(node)
                        self.network_recovery_event.set()
                    else:
                        throw_error = True
            except WaschTimeoutError:
                self.master.log.info("Node has timeouted")
                self.network_recovery_failed_node.append(node)
                await self.reinit_node(node)

        self.network_recovery_in_progress = False
        self.network_recovery_event.set()

        self.recovering_task = None
        # Only throw an error if we actually run from a failed node
        if failednode and throw_error:
            raise WaschOperationInterrupted()

    async def finish_retransmit(self, node):
        """
        Try to finish a retransmit to a certain certain node one last time.
        """
        if node.last_message.retransmit_count \
            > self.master.config.retransmissionlimit:
            node.state = FAILED
            raise WaschTimeoutError()
        else:
            if self.master.message_pending:
                await asyncio.wait_for(self.master.message_event.wait(), 10)
            await node.retransmit()
