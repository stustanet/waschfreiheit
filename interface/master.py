"""
Waschmaster to handle the key network management communicating with all the
nodes - not knowing anything about sensors!
"""

import asyncio
import serial
import serial_asyncio

from exceptions import MasterCommandError
from message import MessageCommand, MessageResponse

class Master:
    """
    Should be properly setup before running, be very carefully if you do not set
    it up from new contextes!
    """
    def __init__(self, loop, serialdevice, baudrate, config):
        self.loop = loop
        self.device = serialdevice
        self.baudrate = baudrate
        self.nodes = {}
        self.id_to_node = {}
        self.config = config
        #self.pluginmanager = None

        self.allow_next_message = True

        self._reader, self._writer = (None, None)

    def add_node(self, node):
        self.nodes[node.name()] = node
        self.id_to_node[node.node_id()] = node

    async def run(self):
        """
        Main run method, should be started as the main task from outside
        """


        #await self.pluginmanager.call("on_start")

        for n in self.nodes.values():
            n.initialize()

        while True:
            await self.connect()

            # reboot the master node to kill any old connections, if this script is restarted
            await self.send("reboot")
            await asyncio.sleep(2)
            await self.init_routes()

            try:
                while True:
                    # Receive serial packet
                    try:
                        line = await asyncio.wait_for(self._reader.readline(), 1)
                        line.decode("ascii")
                        print("[M] RECF: ", line)
                        #await self.pluginmanager.call("on_serial_rx", data=line)
                        self.parse_packet(line)
                    except asyncio.TimeoutError:
                        pass

                    for node in self.nodes.values():
                        print(node.debug_state())
                        if self.allow_next_message:
                            next = node.next_message()
                            if next is not None:
                                await self.send(next)

                    await asyncio.sleep(0.001)

            except serial.SerialException as error:
                #await self.pluginmanager.call("on_serial_error",
                #                              required=True,
                #                              error=error)
                continue # reconnect
            except Exception as error:
                #await self.pluginmanager.call("on_read_error",
                #                              required=True,
                #                              error=error)
                self.loop.stop()
                raise

    async def connect(self):
        """
        connect to the serial device, clear it before if it was strange
        Do not do any setup - this will be done in the base plugin
        """
        #await self.pluginmanager.call("on_before_connect")
        if self._writer:
            self._writer.close()

        self._reader, self._writer = await serial_asyncio.open_serial_connection(
            url=self.device,
            baudrate=self.baudrate,
            loop=self.loop)

        #await self.pluginmanager.call("on_serial_available")

    async def send(self, msg, expect_response=True):
        """
        Send a message to the serial device.
        Does _NOT_ wait for any kind of result!
        """
        self.message_pending = True

        if isinstance(msg, MessageCommand):
            msg = msg.to_command()

        if msg[-1] != "\n":
            msg += "\n"

        print("[M] Sending \"{}\"".format(msg.encode('ascii')))
        #await self.pluginmanager.call("on_serial_tx", data=msg)

        self.allow_next_message = not expect_response
        self._writer.write(msg.encode('ascii'))
        await self._writer.drain()

    def parse_packet(self, packet):
        """
        Prepare the message line for the message parser, removing any possible
        offsets and unnecessary whitespaces
        """
        packet = packet.decode('ascii').strip()
        cmdoffset = packet.find("###")
        if cmdoffset >= 0:
            packet = packet[cmdoffset:]
            msg = MessageResponse(packet)

            if msg.msgtype == 'err':
                raise MasterCommandError("PANIC! We got an ERR response")

            if msg.node in self.id_to_node:
                node = self.id_to_node[msg.node]

                if msg.msgtype == 'ack':
                    node.on_ack(int(msg.result))
                    self.allow_next_message = True
                elif msg.msgtype == 'timeout':
                    node.on_timeout()
                    self.allow_next_message = True
                elif msg.msgtype == 'status':
                    self.status_for_node(node, int(msg.result))
                else:
                    raise MasterCommandError("PANIC! We got an UNKNOWN response")

        #else:
        #    await self.pluginmanager.call("on_nocommand", packet=packet)

    def resolve_node(self, name):
        if name in self.nodes:
            return self.nodes[name]
        return None

    async def init_routes(self):
        routes = []
        for dst, hop in self.config['routes']:
            d = self.resolve_node(dst).node_id()
            h = self.resolve_node(hop).node_id()
            routes += ["{}:{}".format(d, h)]

        routestr = 'routes ' + ','.join(routes)
        await self.send(routestr, False)

    def status_for_node(self, node, status):
        print("Status for node \"{}\" is now {}".format(node.name(), status))

        # Call the status change on ALL nodes, not just on the changed!
        for n in self.nodes.values():
            n.on_node_status_changed(node, status)
