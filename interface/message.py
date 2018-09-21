"""
Message types to abstract the message parsing away
"""

import re

class MessageResponse:
    """
    A message sent by the master node, starting with ###
    """
    def __init__(self, response):
        self.msgtype = None
        self.result = None
        self.node = None
        self.is_error = False
        self.raw = response

        if response:
            self._parse_response(response)

    def _parse_response(self, response):
        match = re.search(r"###(?P<type>ACK|STATUS|ERR|TIMEOUT)[ -]?(?P<node>\d+)"
                          r"(?:[ -](?P<result>\d+))?", response)

        if not match:
            self.is_error = True
            return

        self.msgtype = match.group("type").lower()

        if self.msgtype in ["ack", "status"]:
            self.result = match.group("result")
        elif self.msgtype == "err":
            self.is_error = True

        self.node = int(match.group("node"))

class MessageCommand:
    """
    A message going to the serial connection
    """
    def __init__(self, nodeid, command, *args, seperator=" ", ignore_response=False):
        self.nodeid = nodeid
        self.ignore_response = ignore_response
        if args:
            strargs = seperator.join([str(arg) for arg in args])
        else:
            strargs = ""
        self.cmd = "{} {} {}\n".format(command, nodeid, strargs).strip()

    def __str__(self):
        return self.cmd

    def to_command(self):
        """
        Convert to a interpretable command
        """
        return self.cmd
