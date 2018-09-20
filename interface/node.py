"""
A node within the waschnetwork is a radio-controlled element.
Usually it is connected to some sensors, which send updates on state change

The base node takes care of the basic sensor setup, which includes connect and
route. Any additional system setup has to be done by a follow-up plugin that
sets run_command and start_command.

During system-setup the self.error_state should be updated, whenever a fallback
 stage exists. It defaults to "connect"
"""

import asyncio
import time
import functools

from message import MessageCommand

class Node:
    """
    Implements a Node in the waschnetwork
    This has to be extended by plugins.

    Especially a start_command and a run_command has top be set.
    """

    def __init__(self, config, master, nodeid, name):
        self.name = name
        self.nodeid = nodeid
        self.config = config
        self.master = master

        self.__msgs = []
        self.__message_retry_count = 0
        self.__allow_next_message = True
        self.__last_msg = None

        self._status = self.connect
        self._next_state = self.connect

        self.sent_time = 0

        self.gateway = None
        self.routes = []

        self.error_state = self.connect
        self.run_command = self.void_state
        self.start_command = self.void_state

    def debug_state(self, indent=1, prefix=None):
        """
        Generate a short but conclusive state print to use in the debuginterface
        """
        if not prefix:
            prefix = "[" + str(self.name) + "]"

        if isinstance(self._status, functools.partial):
            name = self._status.func.__name__
        else:
            name = self._status.__name__

        led_state = getattr(self, "_expected_led_state", None)

        #linestart = prefix + ("\t" * indent)
        linestart = "\t"
        return \
            "{prefix}"\
            "{line}status:   {status}\t"\
            "{line}last_msg: {msg}\t"\
            "{line}retry:    {retry}\t"\
            "{line}stalled:  {stalled}\t"\
            "{line}LEDs:     {led}".format(
                prefix=prefix,
                line=linestart,
                status=name,
                msg=self.__last_msg,
                retry=self.__message_retry_count,
                stalled=not self.__allow_next_message,
                led=led_state
            )


    # Message Receiving
    async def recv_msg(self, msg):
        """
        Receive and store the message in the receival cue.
        """
        self.__msgs.append(msg)

    async def iterate(self):
        """
        Process all newly arrived messages, and afterwards tick
        """
        for msg in self.__msgs:
            print("[{}] received: {}".format(self.name, msg.raw))
            await self._process_message(msg)
        self.__msgs = []

        if self.__allow_next_message:
            msg = await self.tick()
            if msg:
                self.__allow_next_message = False
                self.__last_msg = msg
                self.sent_time = time.time()
                await self.master.send(msg)

    async def tick(self):
        """
        Execute the statemachine
        """
        msg = None
        old_state = None
        while self.__allow_next_message \
              and msg is None \
              and self._status != old_state:
            old_state = self._status
            self._status, msg = await self._status()

        return msg

    async def _process_message(self, msg):
        """
        Multiplex between the different types of messages and call the
        plugins accordingly
        """
        if msg.msgtype == "timeout":
            print("Retransmit")
            if self.__message_retry_count > self.config['retry_count']:
                await self.on_connection_lost(msg=self.__last_msg)
            else:
                self.__message_retry_count += 1
                await self.master.send(MessageCommand(self.nodeid, "retransmit"))

        elif msg.msgtype == "ack":
            self.__message_retry_count = 0
            self.__allow_next_message = True
        elif msg.msgtype in ["status"]:
            await self.call("on_msg_" + msg.msgtype, msg=msg)
        elif msg.msgtype == "err":
            # something was bad
            await self.master.plugin_manager.call("on_msg_" + msg.msgtype, msg=msg)
            print("ERR")
        else:
            raise RuntimeError("Received a message that has no known status")

    # Plugin stuff
    async def call(self, fname, required=False, **kwargs):
        """
        Optionally call the given method name, if it si not defined and not
        required, it will not be called
        """
        func = getattr(self, fname, None)

        if func:
            if asyncio.iscoroutinefunction(func):
                return await func(**kwargs)
            return func(**kwargs)
        elif required:
            raise AttributeError("Node {} is missing plugin response {}".format(
                type(self).__name__, fname))

    async def on_connection_lost(self, msg):
        """
        When the connection is lost (aka too many timeouts)
        the state is changed into the error state.
        """
        del msg
        self._status = self.error_state
        # Reset the resend counter
        self.__allow_next_message = True
        self.__message_retry_count = 0

    # State Machine stuff
    async def void_state(self):
        """
        Undefined state - must never ever be entered
        """
        raise NotImplementedError("current state is not defined by the plugin")
        #return self.void_state, None


    async def connect(self):
        """
        Send the "connect" command
        """
        # TODO initial hop
        self.error_state = self.connect
        return self.route, MessageCommand(self.nodeid, "connect", 0)

    async def route(self):
        """
        Send the defined routes
        """
        self.error_state = self.route

        routestr = ",".join("{}:{}".format(hop, dst) for hop, dst in self.routes.items())

        return self.start_command, MessageCommand(self.nodeid, "set_route", routestr)

    async def pingtest(self):
        """
        Send the authping command to the node and return to the run_command
        """
        self._next_state = self.run_command
        self.error_state = self.connect
        return self.run_command, MessageCommand(self.nodeid, "authping")
