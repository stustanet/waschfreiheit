#!/usr/bin/env python3

import time
import json
import sys
import asyncio
import re
import logging
import functools
from serial import SerialException
import serial_asyncio

import sensor
import debuginterface
import uplink

TRANSMITTING = 'transmitting'
CONNECTED = 'connected'
FAILED = 'failed'
TIMEOUTED = 'timeouted'

PRIORITY_RECOVERY = 10
PRIORITY_MESSAGE = 20

class WaschError(Exception):
    """
    Base Wasch Exception, can be used to catch all wasch related errors
    """
    def __repr__(self):
        return "WaschError"


class WaschCommandError(WaschError):
    """
    A Command has returned as an error from the node. Check your parameters, yo
    """
    def __init__(self, command):
        super().__init__()
        self.command = command

    def __repr__(self):
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
            self.do_uplink = conf['webinterface_enabled']
            if self.do_uplink:
                self.uplink_base = conf['webinterface_basepath']
                self.uplink_auth = conf['webinterface_token']
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

    @functools.total_ordering
    class Message:
        """
        Message object that is currently transferred to the node.
        This is used to keep track of the number of retransmits and to make
        shure the messages are well-formed
        """
        def __init__(self, messagestr, node=None, expect_response=True, loop=None,
                     is_recovery=False, **kwargs):
            if not loop:
                loop = asyncio.get_event_loop()
            self.loop = loop
            self.message = messagestr
            self.retransmit_count = 0
            self.retransmit = False
            self.expect_response = expect_response
            self.node = node
            self.is_recovery = is_recovery
            self.result = asyncio.Future(loop=loop)

        def __repr__(self):
            return self.message

        def __lt__(self, other):
            return self.message < other.message

        def __eq__(self, other):
            return self.message == other.message

        def command(self):
            """
            Representation of the command so that it can be issued on the line
            directly
            """
            if self.retransmit:
                return "retransmit {}\n".format(self.node.nodeid)\
                                        .encode('ascii')

            msg = self.message
            if not msg:
                msg = '\n'
            if msg[-1] != '\n':
                msg += '\n'
            return msg.encode('ascii')

    def __init__(self, serial_port, configfile, baudrate=115200, loop=None):
        # Base setup
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop

        # Setup the log
        logformat = '%(asctime)s | %(name)s | %(levelname)5s | %(message)s'
        logging.basicConfig(format=logformat)
        self.log = logging.getLogger('WiF')
        self.log.setLevel(logging.DEBUG)
        self.log.info("starting wasch interface")

        # the configuration to use
        self.config = WaschConfig(configfile, log=self.log)
        # the serial port to use
        self.serial_port = serial_port
        # with the corresponding baud-rate
        self.baudrate = baudrate

        # Setup the uplink connection
        if self.config.do_uplink:
            self.uplink = uplink.WaschUplink(self.config.uplink_base,
                                             self.config.uplink_auth)
        else:
            self.uplink = None

        self._heartbeat_task = None
        # message synchronisation to avoid multiple commands on the line at the
        # same time
        self.message_pending = False
        self.message_event = asyncio.Event(loop=loop)
        self.running_message = None
        # the list of sensors that are currently known and have at least once
        # connected successfully
        self.sensors = {}

        # the queue of messages pending
        self._queue = asyncio.PriorityQueue(loop=loop)

        self.log.info("opening serial connection to %s", serial_port)
        # set up the serial connection
        coro = serial_asyncio.open_serial_connection(
            url=serial_port,
            baudrate=baudrate, loop=loop)
        self.all_reader, self.writer = loop.run_until_complete(coro)

        # the reader will be fed by the connection observer, that will always
        # listen and not miss a ###status update.
        self.raw_reader = None
        self.status_reader = asyncio.StreamReader()
        self.command_reader = asyncio.StreamReader()

        self._network_task = self.loop.create_task(
            self._network_filter_task())

        self._command_task = self.loop.create_task(
            self._process_commands())

        self._status_task = self.loop.create_task(
            self._status_update_task())

        self.networkmanager = NetworkManager(self.config, self)
        self.log.info("connection setup done")

        self.debuginterface = debuginterface.DebugInterface(self)


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

                try:
                    strline = line.decode('ascii')
                except UnicodeError:
                    continue
                if not self.raw_reader is None:
                    self.raw_reader.feed_data(line)


                if re.search(r'###STATUS', strline.upper()):
                    self.status_reader.feed_data(line)
                elif re.search(r'###(ERR|ACK|TIMEOUT)', strline.upper()):
                    if strline[0] != '#':
                        line = line[strline.find('###'):]
                    self.command_reader.feed_data(line)
                elif re.search(r'###', strline):
                    self.log.error("Received strange message %s", strline)
                else:
                    pass
            except SerialException:
                self.log.error("Lost contact to our node. This is bad!")
                self.loop.stop()
                break


    async def _status_update_task(self):
        self.log.info("Started STATUS watcher")
        while True:
            try:
                line = await self.status_reader.readline()

                if self.networkmanager.network_recovery_in_progress:
                    self.log.info("Suspending status update until recovery has finished")
                    await self.networkmanager.network_recovery_event.wait()

                line = line.decode('ascii')
                self.log.info("Processing status message %s", line)
                await self.__status(line[len("###STATUS"):])
            except asyncio.CancelledError:
                raise
            except Exception as exp:
                self.log.error("Exception during status processing")
                self.log.exception(exp)


    async def _process_commands(self):
        """
        Process tasks issued to the master node
        This enforces, that only one message is runnning at a time
        It also manages the retransmissions and marking nodes as failed
        """
        self.log.info("Started command observer")
        while True:
            msg = await self._queue.get()
            msg = msg[1] # remove priority
            self.message_event.clear()
            self.message_pending = True
            self.running_message = msg

            if msg.node:
                msg.node.last_message = msg
                msg.node.state = TRANSMITTING

            msg_ok = False
            retransmissioncount = self.config.retransmissionlimit
            while retransmissioncount >= 0:
                retransmissioncount -= 1

                self.log.debug("SEND %s", msg.command())
                # Now actually send the data
                self.writer.write(msg.command())

                if not msg.expect_response:
                    msg.result.set_result(0)
                    msg_ok = True
                    break

                if await self.__receive_response(msg):
                    msg_ok = True
                    break

            self.message_pending = False
            self.message_event.set()
            if not msg_ok:
                # We have run over retransmit limit.
                if msg.node:
                    msg.node.state = FAILED
                if not msg.is_recovery:
                    self.log.info("Message %s has finally timeouted. "
                                  "Setting node to failed and invoke "
                                  "network check", msg)
                    await self.networkmanager.perform_networkcheck()
                else:
                    self.log.info("Message %s timeouted but during connection "
                                  "setup no recovery will be performed", msg)
                if not msg.result.done():
                    if retransmissioncount < 0:
                        msg.result.set_exception(WaschTimeoutError())
                    else:
                        msg.result.set_exception(WaschOperationInterrupted())


    async def heartbeat(self):
        """
        Send heartbeat that the system is alive to the server
        """
        while True:
            if self.networkmanager.last_observer_run + \
               self.config.networkcheckintervall * 2 \
               > self.config < time.time():
                # We are sad, we do not send a heartbeat
                self.log.error("Last observer run was too old - will not send heartbeat")
            else:
                await self.uplink.heartbeat()

            await asyncio.sleep(60)

    async def start(self):
        """
        Start the networking process
        """
        await self.debuginterface.start()
        await self.networkmanager.init_the_network()
        self._heartbeat_task = self.loop.create_task(self.heartbeat)

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
            self._status_task.cancel()
            await self._status_task
        except asyncio.CancelledError:
            pass

        try:
            self._command_task.cancel()
            await self._command_task
        except asyncio.CancelledError:
            pass

        try:
            if self._heartbeat_task:
                self._heartbeat_task.cancel()
                await self._heartbeat_task
        except asyncio.CancelledError:
            pass

        await self.networkmanager.teardown()
        await self.debuginterface.teardown()


    async def __receive_response(self, msg):
    # Now we start listening for the response, and parse it.
        while True:
            line = await self.command_reader.readline()
            line = line.decode('ascii')

            match = re.findall(
                r'###(ACK|ERR|TIMEOUT)(\d+)(?:-(\d+))?',
                line.upper())
            if match:
                match = match[0]
            else:
                self.log.error("Received an invalid command %s", line)
                continue

            if msg.node:
                if int(match[1]) != msg.node.nodeid:
                    self.log.error("We have received a response for a "
                                   "non-running message!")
                    continue
            if match[0] == 'ACK':
                msg.result.set_result(match[2])
                if msg.node:
                    msg.node.state = CONNECTED
                    if self.uplink:
                        await self.uplink.statistics_update(
                            msg.node, 'ACK - ' + repr(msg))
                return True
            elif match[0] == 'ERR':
                if msg.node:
                    msg.node.state = FAILED
                if self.uplink:
                    await self.uplink.statistics_update(
                        msg.node, 'ERR - ' + repr(msg))
                await self.networkmanager.perform_networkcheck()
                # The message processing is ok, we do not expect any reponse for
                # the last message
                msg.result.set_exception(WaschOperationInterrupted())
                return True
            elif match[0] == 'TIMEOUT':
                self.log.info("Received timeout for message %s", msg)
                if self.uplink:
                    await self.uplink.statistics_update(
                        msg.node, 'TIMEOUT - ' + repr(msg))
                msg.retransmit = True
                msg.retransmit_count += 1
                if msg.node:
                    msg.node.state = TIMEOUTED
                return False
            else:
                self.log.error('received invalid command %s', match[0])
                continue


    async def send_raw(self, msg, node, *args, **kwargs):
        """
        Prepare a message to be sent

        kwargs include:
        * expect_response=True
        * node=None
        * is_recovery=False
        """
        msg = WaschInterface.Message(msg, loop=self.loop, node=node, **kwargs)
        priority = PRIORITY_MESSAGE
        try:
            if kwargs['is_recovery']:
                priority = PRIORITY_RECOVERY
        except KeyError:
            pass
        try:
            await self._queue.put((priority, msg))
        except TypeError:
            # The object is already in the queue - so it is save to ignore
            pass
        await msg.result


    async def node(self, nodeid):
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

        self.log.info("Initing node %s (%i): distance: %i",
                      confignode.name, nodeid, confignode.distance)

        # If we insert the new sensor here, it is be added to the recovery in
        # in case it is not successfull to connect
        # TODO: Remove the node? Why not only resset?
        newsensor = sensor.Sensor(self, nodeid, confignode, loop=self.loop)
        if nodeid in self.sensors:
            del self.sensors[nodeid]

        self.sensors[nodeid] = newsensor
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
            try:
                return self.sensors[int(match[0])]
            except (KeyError, TypeError):
                self.log.error("Could not find sensor with ID %s", match[0])
                raise KeyError()
        else:
            raise KeyError()


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
            try:
                node = self.get_node(int(nodeid))
                if self.uplink:
                    await self.uplink.status_update(node, status)
                await node.notify_status(status)
            except WaschTimeoutError:
                self.log.warning("Timeout during status processing... "
                "That is too high packet loss. go and die")
            except WaschError as exp:
                self.log.error("Wasch Error during status processign - what has"
                               " happened? %s", repr(exp))

            except KeyError:
                self.log.error("I think there was a status update for a not "
                               "configured node")
        else:
            raise WaschCommandError("Wasch Interface send invalid status "
                                    "command")



    ############################################################################
    ## Now we have commands
    ############################################################################
    async def sensor_config(self, node, key_status, key_config, **kwargs):
        """
        node: id of the node to configure
        key_status: common key for the status channel
        key_config: common key for the configuration channel
        both keys have to be 16 bytes long.
        """
        await self.send_raw("config {} {} {}"
                            .format(node, key_status, key_config),
                            None, **kwargs)


    async def sensor_routes(self, routes, **kwargs):
        """
        MASTER ONLY: Set the routes of a sensor node

        Routes in the format : {dst1: hop1, dst2: hop2}
        """
        routestring = ",".join(["{}:{}".format(dst, hop)
                                for dst, hop in routes.items()])
        await self.send_raw("routes {}".format(routestring), None,
                            expect_response=False, **kwargs)


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

        self.network_sanity_requested = asyncio.Event(loop=loop)
        self.network_sanity_observer_task = loop.create_task(
            self.network_sanity_observer())
        self.last_observer_run = None
        self.next_observer_run = None

    async def teardown(self):
        try:
            self.network_sanity_observer_task.cancel()
            await self.network_sanity_observer_task
        except asyncio.CancelledError:
            pass


    async def perform_networkcheck(self):
        """
        Force a networkcheck
        """
        if self.network_sanity_requested.is_set():
            self.master.log.info("Networkcheck was requested, but there is "
                                 "already a networkcheck in progress")

        self.network_sanity_requested.set()


    async def network_sanity_observer(self):
        """
        In regular intervals start a network recovery task.
        The network recovery starts with pinging all nodes an assessing, what
        part of the network does neet do be recovered, so it gets in contact
        with every node

        This should not be done too often, since it requires a lot of bandwidth
        """
        self.master.log.info("Start network sanity observer every %s seconds",
                             self.master.config.networkcheckintervall)

        while True:
            timeout = self.master.config.networkcheckintervall
            self.next_observer_run = time.time() + timeout

            try:
                self.network_sanity_requested.clear()
                await asyncio.wait_for(self.network_sanity_requested.wait(), timeout)
            except asyncio.TimeoutError:
                pass

            try:
                self.master.log.info("Performing network sanity check")
                await self.recover_network()
            except WaschOperationInterrupted:
                print("Interrupt")
            except WaschCommandError as exp:
                self.master.log.exception(exp)
            self.last_observer_run = time.time()


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


    async def init_the_network(self, confignode=None):
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
        if not confignode:
            confignode = self.config.nodes["MASTER"]

        if confignode.is_initialized:
            self.master.log.debug("Skipping initialized node %s", confignode.name)
            return

        if confignode.name == 'MASTER':
            await self.master.sensor_routes(confignode.routes_id, is_recovery=True)
            confignode.is_initialized = True
            for nodename in confignode.routes.values():
                await self.init_the_network(self.config.nodes[nodename])
        else:
            try:
                node = self.master.get_node(confignode.name)
            except KeyError:
                node = await self.master.node(confignode.id)

            self.master.log.info("Initializing node %s", confignode.name)
            await self._initialize_single_node(node)


    async def _initialize_single_node(self, node):
        if node.config.name != 'MASTER':
            self.master.log.info("Connecting to node %s (%i)",
                                node.config.name, node.nodeid)
            try:
                timeout = self.master.config.single_hop_timeout * node.config.distance
                nexthop = self.config.nodes[node.config.routes["MASTER"]].id
                node.config.is_initialized = True
                await node.connect(nexthop, timeout, is_recovery=True)

                if node.state == CONNECTED:
                    await node.routes(node.config.routes_id, reset=True,
                                      is_recovery=True)
            except WaschError as exp:
                if self.master.uplink:
                    await self.master.uplink.statistics_update(node, 'CONNECTFAIL')
                self.master.log.error("Could not connect to %s (%i): %s "
                                      "Check the hardware, then the routes.",
                                      node.config.name, node.nodeid, repr(exp))
                node.state = FAILED
                return

            try:
                # Send sensor config data,
                bitmask = 0
                for channel in node.config.channels.values():
                    bitmask += 2**int(channel.number)
                    await node.configure(
                        channel.number,
                        *self.parse_sensor_config(channel.config))

                await node.enable(bitmask, node.config.samplerate,
                                  is_recovery=True)
                await node.resend_state()
            except WaschError as exp:
                self.master.log.warning(
                    "Could connect to %s (%i) but not initialize: %s",
                    node.config.name, node.nodeid, repr(exp))
                node.state = FAILED
            if node.state == FAILED:
                await self.perform_networkcheck()
        else:
            self.master.log.info('Initializing first round of nodes')
        # try to initialize all nodes adjacent to this one
        for nodename in node.config.routes.values():
            try:
                if not self.config.nodes[nodename].is_initialized:
                    await self.init_the_network(self.config.nodes[nodename])
            except WaschOperationInterrupted:
                pass


    async def recover_network(self):
        """
        Perform a network recovery

        Perform the following pseudo codes

        ```
        for all nodes ordered by their distance:
            if ping() successfull
`                continue
            else
                connect()
`       ```

        That way this function can be used to scan the network for proper
        operation as well as the recovery if a node has failed
        """

        self.network_recovery_event.clear()
        self.network_recovery_in_progress = True
        self.master.log.info("Scanning Network for proper operation")

        if self.master.message_pending and self.master.running_message.node:
            timeout = self.master.running_message.node.distance \
                      * self.config.single_hop_timeout + 2
            self.master.log.info("Waiting for a missing response for %s "
                                 "for %s seconds",
                                 self.master.running_message, timeout)
            try:
                self.master.message_event.clear()
                #await asyncio.wait_for(
                #    self.master.message_event.wait(),
                #    timeout)
                await self.master.message_event.wait()
            except asyncio.TimeoutError:
                self.master.log.error("Needed to kill a running transaction")
                self.master.log.error("We need to reboot the master!")
                sys.exit(-1)
                self.master.message_pending = False

        # Do we have to raise WaschOperationInterrupted error after we're done?
        if not self.master.sensors:
            self.master.log.error("Nodelist is empty at the moment, which "
                                  "means it was not yet set up properly.")
            return
            #raise WaschOperationInterrupted()
        self.master.log.info(
            "No transmissions are pending, proceeding with %s nodes: %s",
            len(self.master.sensors),
            ', '.join([n.config.name
                       for n in self.master.sensors.values()]))
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
                    # If this tries to start a network recovery, it will raise
                    # a timeout error instead.
                    self.master.log.info("Waiting for the node to finish the "
                                         " transmission for %s seconds",
                                         timeout)
                    try:
                        await asyncio.wait_for(self.finish_retransmit(node),
                                               timeout)
                        self.master.log.info("Retransmit successfull")
                        # If this completed successfully, we do not want to die
                    except (WaschTimeoutError, asyncio.TimeoutError):
                        self.master.log.info("Node has timeouted.")
                        await self._initialize_single_node(node)

                elif node.state == CONNECTED:
                    # If a node is connected, we try to ping it, and if this is
                    # ok, we can relax
                    self.master.log.info(
                        "Pinging node %s (%i), timeout: %i",
                        node.config.name, node.nodeid, timeout)
                    await node.authping()
                    self.master.log.info("ping successfull")
                elif node.state == FAILED:
                    if self.master.uplink:
                        await self.master.uplink.statistics_update(node, 'FAIL-RESCUE')
                    self.master.log.info("Restarting failed Node %s (%i)",
                                         node.config.name, node.nodeid)
                    await self._initialize_single_node(node)
                else:
                    self.master.log.error(
                        "Node %s (%i) is in invalid state: %s",
                        node.config.name, node.nodeid, node.state)
                    node.state = FAILED
            except WaschError:
                self.master.log.warning("Network check has failed for %s (%i)",
                                        node.config.name, node.nodeid)
                node.state = FAILED
                await self._initialize_single_node(node)

        # Done recovering
        self.network_recovery_in_progress = False
        self.network_recovery_event.set()


    async def finish_retransmit(self, node):
        """
        Try to finish a retransmit to a certain certain node one last time.
        """
        if node.last_message.retransmit_count \
            > self.master.config.retransmissionlimit:
            node.state = FAILED
            self.master.log.debug("Node was already over retransmission limit")
            raise WaschTimeoutError()
        else:
            if self.master.message_pending:
                #await asyncio.wait_for(self.master.message_event.wait(), 10)
                await self.master.message_event.wait()
            if self.master.message_pending:
                await node.retransmit()
