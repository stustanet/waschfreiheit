"""
Waschmaster to handle the key network management communicating with all the
nodes - not knowing anything about sensors!
"""

import time
import asyncio
import serial
import serial_asyncio
import socket
import logging

from exceptions import MasterCommandError
from message import MessageCommand, MessageResponse

class Master:
    """
    Should be properly setup before running, be very carefully if you do not set
    it up from new contextes!
    """
    def __init__(self, loop, config, uplink):
        self.loop = loop
        self.nodes = {}
        self.id_to_node = {}
        self.config = config
        self.uplink = uplink
        self.injected_command = None
        self.raw_mode = False
        self.restart_requested = False
        self.debug_interface = None

        self.alive = False
        self.last_cmd = None
        self.initialized = False
        self.tcp_server = None
        self.new_con_evt = None
        self.last_node_commands = {}

        # Set when received a PEND signal
        # Cleared when received ACK or TIMEOUT signal
        self.message_pending = False

        # Set to the name of the node when a command is sent
        # Cleared to None when the command prompt is read and the master node is ready for the next command
        self.wait_for_prompt = None

        self._reader, self._writer = (None, None)
        self.log = logging.getLogger('master')

    def add_node(self, node):
        self.nodes[node.name()] = node
        self.id_to_node[node.node_id()] = node

    def set_debug_interface(self, inter):
        self.debug_interface = inter

    async def run(self):
        """
        Main run method, should be started as the main task from outside
        """

        last_alive_signal = 0
        last_wdt_feed = 0

        if self.debug_interface is not None:
            await self.debug_interface.start()

        while True:
            for n in self.nodes.values():
                n.initialize()
            self.injected_command = None
            self.raw_mode = False
            self.message_pending = False
            self.wait_for_prompt = None

            self.initialized = True

            await self.connect()


            try:
                # reboot the master node to kill any old connections, if this script is restarted
                await self.send("reboot")
                await asyncio.sleep(2)
                await self.init_routes()
                await asyncio.sleep(1)

                self.message_pending = False
                self.wait_for_prompt = None

                startup_time = time.monotonic()

                while True:
                    # Receive serial packet
                    try:
                        line = await asyncio.wait_for(self._reader.readline(), 1)
                        if not line:
                            print("Received empty data!")
                            break

                        print("RECV '{}'".format(line.decode("ascii").strip()))
                        self.log.debug("RECV '{}'".format(line.decode("ascii").strip()))
                        if self.debug_interface is not None:
                            self.debug_interface.send_text(line.decode("ascii"), True)

                        self.parse_packet(line)
                    except asyncio.TimeoutError:
                        pass

                    if self.restart_requested:
                        self.restart_requested = False
                        # just break, the outer loop will do the restart
                        break

                    if self.injected_command is not None:
                        await self.send(self.injected_command)
                        self.injected_command = None

                    now = time.monotonic()

                    if now < startup_time + 1:
                        # Wait for 1 sec at start to discard any messsaages from the initialization
                        # before beginning to send messages to the nodes
                        continue

                    if last_wdt_feed + self.config['gateway_watchdog_interval'] < now:
                        self.log.debug("Feeding gateway watchdog")
                        await self.send("wdt_feed")
                        last_wdt_feed = now

                    if not self.raw_mode and (self.wait_for_prompt is not None or self.message_pending):
                        # Waiting for the last message to be processed or for the timeout / ack for the last command
                        continue

                    #print(self.debug_state())
                    self.alive = False
                    for node in self.nodes.values():
                        if node.is_available():
                            self.alive = True

                        if not self.raw_mode:
                            next = node.next_message()
                            if next is not None:
                                self.last_node_commands[node.name()] = next.to_command()
                                await self.send(next)
                                break

                    if last_alive_signal + self.config['alive_signal_interval'] < now:
                        if self.alive:
                            self.uplink.send_alive_signal()
                            last_alive_signal = now


                    await asyncio.sleep(0.001)

            except ConnectionResetError:
                print("Connection reset")
                continue # reconnect
            except serial.SerialException:
                print("Serial error")
                continue # reconnect
            except socket.error:
                print("Socket error")
                continue # reconnect
            except Exception as error:
                #await self.pluginmanager.call("on_read_error",
                #                              required=True,
                #                              error=error)
                self.loop.stop()
                raise

    async def handle_tcp_con(self, rd, wr):
        if self.new_con_evt.is_set():
            # Don't want to accept a new connection
            # just close the con
            wr.close()
            return

        #else: set the new reader and writer of the master and set the signal
        self._reader = rd
        self._writer = wr
        self.new_con_evt.set()

    async def connect(self):
        """
        connect to the serial device or wait for a socket connection
        """

        if self._writer:
            self._writer.close()

        if self.config['connection'] == 'serial':
            device = self.config['serial']['device']
            baudrate = self.config['serial']['baudrate']

            self._reader, self._writer = await serial_asyncio.open_serial_connection(
                url=device,
                baudrate=baudrate,
                loop=self.loop)
        elif self.config['connection'] == 'tcp':
            if self.tcp_server is None:
                # Start the server
                port = self.config['tcp']['port']
                self.new_con_evt = asyncio.Event()
                self.tcp_server = await asyncio.start_server(self.handle_tcp_con, '0.0.0.0', port)

            self.log.info("Waiting for connection on port {}".format(self.config['tcp']['port']))

            self.new_con_evt.clear()
            await self.new_con_evt.wait()

            self.log.info("Got connection from {}".format(self._writer.get_extra_info('peername')))

            # reset the MCU and start forwarding
            self._writer.write(b'reset\n')
            await self._writer.drain()
            await asyncio.sleep(1)
            self._writer.write(b'forward\n')
            await self._writer.drain()


        #await self.pluginmanager.call("on_serial_available")

    async def send(self, msg):
        """
        Send a message to the serial device.
        Does _NOT_ wait for any kind of result!
        """


        if isinstance(msg, MessageCommand):
            self.wait_for_prompt = msg.node
            msg = msg.to_command()
        else:
            self.wait_for_prompt = "MASTER"

        if msg[-1] != "\n":
            msg += "\n"

        print("[M] Sending \"{}\"".format(msg.encode('ascii')))
        self.log.debug("SEND '{}'".format(msg))
        #await self.pluginmanager.call("on_serial_tx", data=msg)
        if self.debug_interface is not None:
            self.debug_interface.send_text("  -->" + msg, True)

        self.last_cmd = msg[:-1]
        self._writer.write(msg.encode('ascii'))
        await self._writer.drain()

    def parse_packet(self, packet):
        """
        Prepare the message line for the message parser, removing any possible
        offsets and unnecessary whitespaces
        """

        packet = packet.decode('ascii').strip()

        if packet.find("MASTER>") >= 0:
            self.wait_for_prompt = None
            return

        cmdoffset = packet.find("###")
        if cmdoffset >= 0:
            packet = packet[cmdoffset:]
            msg = MessageResponse(packet)

            if msg.is_error and not self.raw_mode:
                if self.wait_for_prompt is None:
                    self.log.error("Got error response '{}'".format(packet))
                    raise MasterCommandError("PANIC! We got an out-of-order ERR response")
                nd = self.resolve_node(self.wait_for_prompt)
                if nd is not None:
                    nd.command_aborted()

                return

            if msg.node in self.id_to_node:
                node = self.id_to_node[msg.node]

                if msg.msgtype == 'ack':
                    if not self.raw_mode:
                        if node.name() in self.last_node_commands:
                            self.uplink.on_serial_status(node.name(), "ACK - " + self.last_node_commands[node.name()])
                        node.on_ack(int(msg.result))
                        self.message_pending = False
                elif msg.msgtype == 'timeout':
                    if not self.raw_mode:
                        if node.name() in self.last_node_commands:
                            self.uplink.on_serial_status(node.name(), "TIMEOUT - " + self.last_node_commands[node.name()])
                        node.on_timeout()
                        self.message_pending = False
                elif msg.msgtype == 'status':
                    if node.name() in self.last_node_commands:
                        self.uplink.on_serial_status(node.name(), packet)
                    self.status_for_node(node, int(msg.result))
                elif msg.msgtype == 'pend':
                    # once we get a PEND event, we block the execution until we get a timeout or a ack
                    if self.wait_for_prompt is None:
                        self.log.warning("Received unexpected pending signal")

                    self.message_pending = True
                else:
                    self.log.err("Got unknown response '{}'".format(packet))
                    raise MasterCommandError("PANIC! We got an UNKNOWN response")

        #else:
        #    await self.pluginmanager.call("on_nocommand", packet=packet)

    def resolve_node(self, name):
        if name in self.nodes:
            return self.nodes[name]
        elif name == 'MASTER':
            return None
        raise KeyError("No such node!", name)

    async def init_routes(self):
        routes = []
        for dst, hop in self.config['routes']:
            d = self.resolve_node(dst).node_id()
            h = self.resolve_node(hop).node_id()
            routes += ["{}:{}".format(d, h)]

        routestr = 'routes ' + ','.join(routes)
        await self.send(routestr)

    def status_for_node(self, node, status):
        self.log.info("Status for node \"{}\" is now {}".format(node.name(), status))

        # Call the status change on ALL nodes, not just on the changed!
        for n in self.nodes.values():
            n.on_node_status_changed(node, status)

    def debug_state(self):
        state = """alive:       {}
raw:         {}
last_cmd:    {}
msg_pending: {}
wait_prompt: {}

""".format(self.alive, self.raw_mode, self.last_cmd, self.message_pending, self.wait_for_prompt)
        for node in self.nodes.values():
            state += node.debug_state() + "\n"
        return state


    def inject_command(self, cmd):
        self.injected_command = cmd

    def set_raw_mode(self, raw):
        self.raw_mode = raw

    def is_raw_mode(self):
        return self.raw_mode

    def request_restart(self):
        self.restart_requested = True

    def reset_timeouts(self):
        for n in self.nodes.values():
            n.reset_timeout()
