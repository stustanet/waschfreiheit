"""
Do the magic debugging stuff
"""
import asyncio

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

        self.all_sockets = []
        self.commands = {
            'help': self.send_help,
            'raw': self.enable_raw,
            'unraw': self.disable_raw,
            'led': self.led,
            'frames': self.frames,
            'status': self.status,
            'ping': self.ping,
            'check': self.check,
            'restart': self.restart,
        }

        self.server = None
        self.master.set_debug_interface(self)

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
        self.all_sockets += [writer]
        try:
            while True:
                line = await reader.readline()
                linestr = line.decode('ascii').strip()
                if linestr == "":
                    continue

                cmdstr = linestr.split()[0]

                if len(cmdstr) > 1 and cmdstr[0] == '\\':
                    if not self.master.is_raw_mode():
                        writer.write(b"Need to be in raw mode in order to send raw data\n")
                    else:
                        self.master.inject_command(linestr[1:])
                elif cmdstr in self.commands:
                    cmd = self.commands[cmdstr]
                    cmd(linestr, reader, writer)
                else:
                    writer.write(b"Unknown command\n")
        except ConnectionResetError:
            pass
        finally:
            self.all_sockets = [sock for sock in self.all_sockets if sock != writer]
        print("/////////////////////////////////DISCONNECT////////////////////////////")


    def send_text(self, text):
        for s in self.all_sockets:
            s.write(text.encode())


    def send_help(self, _, reader, writer):
        """
        Send help text to the weary traveller
        """
        writer.write(b"""Hello friend!

Welcome to the Debug Interface of the waschmaschinen.
Supported commands are:

""")
        writer.write('\n'.join(self.commands.keys()).encode())
        writer.write(b"""
Have fun.
##
""")

    def enable_raw(self, _, reader, writer):
        self.send_text("""!!! RAW MODE ENABLED !!!
You can send raw data by adding the '\\' prefix.
""")
        self.master.set_raw_mode(True)

    def disable_raw(self, _, r, writer):
        self.send_text("""!!! RAW MODE DISABLED !!!
Restart the master now unless you are ABSOLUTELY SURE that the current state matches the state before the raw mode!
""")
        self.master.set_raw_mode(False)

    def led(self, line, reader, writer):
        print(line)
        self.send_command(line, reader, writer)

    def frames(self, line, reader, writer):
        command = ' '.join(line.split()[1:])
        self.send_command('raw_frames ' + command, reader, writer)

    def status(self, line, reader, writer):
        command = ' '.join(line.split()[1:])
        self.send_command('raw_status ' + command, reader, writer)

    def ping(self, line, reader, writer):
        self.send_command(line, reader, writer, direct=True)

    def check(self, line, reader, writer):
        parts = line.split()
        if len(parts) != 2:
            writer.write(b"### invalid syntax\n")
        command = parts[0]
        nodestr = parts[1]

        try:
            node = self.master.resolve_node(nodestr)
        except KeyError:
            writer.write(b"### could not find node!\n")
            return

        if node is None:
            writer.write(b"### invalid node\n")
            return

        writer.write(b"Request node check\n")
        node.check_con()

    def restart(self, line, reader, writer):
        self.send_text("MASTER RESTART")
        self.master.request_restart()


    def send_command(self, line, r, writer, direct=False):
        """
        Send a command to the master
        """
        parts = line.split()

        if len(parts) < 2:
            writer.write(b"### missing node argument\n")

        command = parts[0]
        nodestr = parts[1]

        try:
            node = self.master.resolve_node(nodestr)
        except KeyError:
            writer.write(b"### could not find node!\n")
            return

        if node is None:
            writer.write(b"### invalid node\n")
            return

        if direct:
            cmd = ' '.join([command, str(node.node_id())] + parts[2:])
            self.master.inject_command(cmd)
            return

        if not node.can_inject_command():
            writer.write(b"### this node cannot accept commands right now! please try again later or use the raw mode\n")
            return

        node.inject_command(command, ' '.join(parts[2:]))
