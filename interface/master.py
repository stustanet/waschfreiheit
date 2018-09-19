"""
Waschmaster to handle the key network management communicating with all the
nodes - not knowing anything about sensors!
"""

import asyncio
import serial
import serial_asyncio

from message import MessageCommand, MessageResponse

class Master:
    """
    Should be properly setup before running, be very carefully if you do not set
    it up from new contextes!
    """
    def __init__(self, loop, serialdevice, baudrate):
        self.loop = loop
        self.device = serialdevice
        self.baudrate = baudrate
        self.nodes = []
        self.pluginmanager = None

        self._reader, self._writer = (None, None)

    async def run(self):
        """
        Main run method, should be started as the main task from outside
        """
        self.nodes = {node.nodeid: node for node in self.pluginmanager.nodes}

        await self.pluginmanager.call("on_start")

        while True:
            await self.connect()
            try:
                while True:
                    # Receive serial packet
                    try:
                        line = await asyncio.wait_for(self._reader.readline(), 1)
                        line.decode("ascii")
                        await self.parse_packet(line)
                    except asyncio.TimeoutError:
                        pass

                    # TODO this has to be made more fancy - with scheduling,
                    #blackjack and hookers
                    for node in self.nodes.values():
                        print(node.debug_state())
                        await node.iterate()

                    await asyncio.sleep(0.1)
            except serial.SerialException as error:
                await self.pluginmanager.call("on_serial_error",
                                              required=True,
                                              error=error)
                continue # reconnect
            except Exception as error:
                await self.pluginmanager.call("on_read_error",
                                              required=True,
                                              error=error)
                self.loop.stop()
                raise

    async def connect(self):
        """
        connect to the serial device, clear it before if it was strange
        Do not do any setup - this will be done in the base plugin
        """
        await self.pluginmanager.call("on_before_connect")
        if self._writer:
            self._writer.close()

        self._reader, self._writer = await serial_asyncio.open_serial_connection(
            url=self.device,
            baudrate=self.baudrate,
            loop=self.loop)

        await self.pluginmanager.call("on_serial_available")

    async def send(self, msg):
        """
        Send a message to the serial device.
        Does _NOT_ wait for any kind of result!
        """
        if isinstance(msg, MessageCommand):
            msg = msg.to_command()
        if msg[-1] != "\n":
            msg += "\n"
        print("[M] Sending \"{}\"".format(msg.strip()))
        self._writer.write(msg.encode('ascii'))
        await self._writer.drain()

    async def parse_packet(self, packet):
        """
        Prepare the message line for the message parser, removing any possible
        offsets and unnecessary whitespaces
        """
        packet = packet.decode('ascii').strip()
        cmdoffset = packet.find("###")
        if cmdoffset >= 0:
            packet = packet[cmdoffset:]
            msg = MessageResponse(packet)

            node = self.nodes.get(msg.node, None)

            if node:
                await node.recv_msg(msg)
        else:
            await self.pluginmanager.call("on_nocommand", packet=packet)
