"""
Do the magic debugging stuff
"""
import asyncio
import re
from . import sensor

class DebugInterface:
    """
    Create a connection for debuggers to connect to and display all the
    raw magic
    """

    def __init__(self, master, loop=None):
        """
        Create a new debuginterface
        Start the raw reader muxing task
        """
        if loop is None:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.master = master
        self.raw_reader = asyncio.StreamReader()

        self.master.raw_reader = self.raw_reader

        self._raw_mux_sockets = []
        self.commands = {
            'help': self.send_help,
            'raw': self.enable_raw,
            'unraw': self.disable_raw,
            'send': self.send_command,
            'led': self.led,
            'frames': self.frames,
            'status': self.status,
            'shutdown': self.shutdown,
        }

        self.raw_reader_mux_task = self.loop.create_task(
            self.raw_reader_muxer())
        self.server = None

    # LED \send led
    # raw_frames \send raw_frames
    # raw_status \send raw_status
    # shutdown (wartung)

    async def start(self):
        """
        Start to handle reqeuests on the socket
        """
        self.server = await asyncio.start_server(
            self._connection_handler,
            '0.0.0.0', 1337, loop=self.loop)

    async def teardown(self):
        """
        Stop the asyncio stuff
        """
        self.raw_reader_mux_task.cancel()
        try:
            await self.raw_reader_mux_task
        except asyncio.CancelledError:
            pass

    async def raw_reader_muxer(self):
        """
        Send a received line to all subscribed raw sockets
        """
        while True:
            line = await self.raw_reader.readline()
            for sock in self._raw_mux_sockets:
                if isinstance(sock, asyncio.StreamReader):
                    sock.feed_data(line)
                elif isinstance(sock, asyncio.StreamWriter):
                    sock.write(line)
                else:
                    self.master.err("Unsupported type!!!\n")

    async def _connection_handler(self, reader, writer):
        """
        Handle an incoming connection, send the welcome texts and stuff
        """
        self.send_help('', reader, writer)
        writer.drain()
        try:
            while True:
                line = await reader.readline()
                linestr = line.decode('ascii').strip()
                if str.startswith(linestr, '\\'):
                    cmdstr = linestr[1:].split()[0]
                    if cmdstr in self.commands:
                        cmd = self.commands[cmdstr]
                        if asyncio.iscoroutine(cmd):
                            await cmd(linestr[1:], reader, writer)
                        else:
                            cmd(linestr[1:], reader, writer)
                    else:
                        writer.write(b"Unknown command\n")
                else:
                    writer.write(b"Start your commands with \\\n")
                await writer.drain()
        except ConnectionResetError:
            pass
        finally:
            self.remove_raw_output(writer)


    def remove_raw_output(self, writer):
        """
        Remove the socket from the raw socket list
        """
        self._raw_mux_sockets = [sock for sock in self._raw_mux_sockets
                                 if sock != writer]

    def send_help(self, _, reader, writer):
        """
        Send help text to the weary traveller
        """
        writer.write(b"""Hello friend!

Welcome to the Debug Interface of the waschmaschinen.
Issue your command using \\[command]
Supported commands are:

""")
        writer.write('\n'.join(self.commands.keys()).encode())
        writer.write(b"""
Have fun.
##
""")

    def enable_raw(self, _, reader, writer):
        """
        Add the sink to the list of outputs
        """
        if writer not in self._raw_mux_sockets:
            self._raw_mux_sockets.append(writer)
            writer.write(b"Enabled raw output for you\n")

    def disable_raw(self, _, r, writer):
        """
        Remove the sing from the list of outputs
        """
        self.remove_raw_output(writer)
        writer.write(b"Disabled raw output for you\n")

    def led(self, line, reader, writer):
        print(line)
        self.send_command('send ' + line, reader, writer)

    def frames(self, line, reader, writer):
        command = ' '.join(line.split()[1:])
        self.send_command('send raw_frames ' + command, reader, writer)

    def status(self, line, reader, writer):
        command = ' '.join(line.split()[1:])
        self.send_command('send raw_status ' + command, reader, writer)

    def shutdown(self, l, reader, writer):
        writer.write(b"# I am now blanking all LEDs\n")
        asyncio.ensure_future(writer.drain())
        for node in self.master.sensors.values():
            asyncio.ensure_future(node.led([sensor.LED.OFF] * 5))
        writer.write(b"# Shutdown complete\n")
        asyncio.ensure_future(writer.drain())


    def send_command(self, line, r, writer):
        """
        Send a command to the master
        """
        command = ' '.join(line.split()[1:])
        serial_command = command.split()[0]
        expect_response = serial_command in [
            "connect", "retransmit", "reset_routes", "set_routes", "cfg_sensor"
            "enable_sensor", "raw_frames", "raw_status", "led"
        ]

        if expect_response:
            match = re.findall(r'(\d+)', line)
            if match:
                try:
                    node = self.master.get_node(match[0])
                except KeyError:
                    writer.write(b"### could not find node!\n")
                    asyncio.ensure_future(writer.drain())
                    return
            else:
                writer.write(b"### please provide a target node!\n")
                asyncio.ensure_future(writer.drain())
                return
        else:
            node = None

        asyncio.ensure_future(self.master.send_raw(
            command,
            node,
            expect_response=expect_response))
