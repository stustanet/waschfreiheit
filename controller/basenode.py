# This is the basic nod implementation
# It manages the basic network functions like the route setup and the keepalive.
# Any other node types should be derived from this class

import time
import logging
from exceptions import NodeStateError
from message import MessageCommand

def now():
    return time.clock_gettime(time.CLOCK_MONOTONIC)

class BaseNode:
    def __init__(self, config, master, name):
        self._config = config
        self._master = master
        self._name = name
        self._node_id = int(config['id'])
        self._max_rt = int(config['max_retransmissions'])
        self._check_interval = int(config['check_interval'])
        master.add_node(self)
        self.log = logging.getLogger('node-{}'.format(name))

    def initialize(self):
        self.log.info('initialize node')
        self._gateway = self._master.resolve_node(self._config['gateway'])

        self._status = {"CON" : False,
                        "CHECK" : False,
                        "RT" : False,
                        "ROUTES" : False,
                        "INITDONE" : False,
                        "REBUILD_SCH" : False}

        # Number of retransmisions
        self._rt_count = 0

        # This is either None (no pendng message)
        # or a tuple (k, v, cb) which defines the change in the status field when the current message is ack'ed
        # cb is a callback that is called on ack
        self._status_on_ack = None

        self._injected_command = None

        self._wait_until = 0
        self._last_ack = 0

        self._initialize()

    def next_message(self):
        if self._wait_until > now():
            # Still waiting
            return None

        if self._gateway is not None and not self._gateway.is_available():
            # My gateway is inactive, can't do anything
            return None

        if self._status['RT']:
            # Need to send retransmit
            self._status['RT'] = False
            self._rt_count += 1
            return MessageCommand(self._node_id, "retransmit")

        if self._status_on_ack is not None:
            # Some command is pending, nothing to do right now
            return None


        if self._injected_command is not None:
            # Command injected by debug interface
            tmp = self._injected_command
            self._injected_command = None
            self._status_on_ack = (None, None, None)
            return tmp

        if not self._status['CON']:
            self.log.info('connecting to node')
            # Not connected, need to connect first
            self._status_on_ack = ("CON", True, BaseNode.__on_connected)
            gw = self._gateway._node_id if self._gateway is not None else 0
            return MessageCommand(self._node_id, "connect", gw, int(self._config['hop_timeout']) * self.route_length())


        if not self._status['ROUTES']:
            # Init the routes
            self._status_on_ack = ("ROUTES", True, None)
            return self.__make_route_msg()

        if self._status['CHECK'] or self._last_ack + self._check_interval < now():
            # Do a ping check
            self.log.debug('checking node still connected')
            self._status_on_ack = ("CHECK", False, None)
            return MessageCommand(self._node_id, "authping")

        if self._status['REBUILD_SCH']:
            # Rebuild the status channel
            self._status_on_ack = ("REBUILD_SCH", False, None)
            return MessageCommand(self._node_id, "rebuild_status_channel")

        return self._next_message()


    def on_ack(self, code):
        if self._status_on_ack is None:
            raise NodeStateError("Got ACK but has no outstanding message!")

        # Update the status
        (k, v, cb) = self._status_on_ack
        if k is not None:
            self._status[k] = v

        if cb is not None:
            cb(self, code)

        self._status['RT'] = False
        self._status['CHECK'] = False
        self._status_on_ack = None
        self._rt_count = 0
        self._last_ack = now()

    def on_timeout(self):
        if self._rt_count > self._max_rt:
            # node timeouted
            if not self._status['CON']:
                # Bad, timeouted but was not connected -> wait some time
                self.log.warning('connection failed, wait {} sec before next attempt'.format(self._config['reconnect_delay']))
                self._wait_until = now() + int(self._config['reconnect_delay'])
                self._on_connection_failed()

            # Need to reconnect
            self._status['CON'] = False

            # Clear pending status
            self._status_on_ack = None

            self.check_con(check_path=True)
            self._rt_count = 0
        else:
            self._status['RT'] = True

    def is_available(self):
        if self._gateway is not None and not self._gateway.is_available():
            return False

        return self._status['CON'] and self._status['ROUTES'] and not self._status['CHECK']

    def check_con(self, check_path=False):
        self._status['CHECK'] = True
        if check_path and self._gateway is not None:
            self._gateway.check_con()

    def node_id(self):
        return self._node_id

    def name(self):
        return self._name

    def route_length(self):
        if self._gateway is not None:
            return self._gateway.route_length() + 1

        return 1

    def debug_state(self):
        if self._last_ack == 0:
            la = "None"
        else:
            la = str(int(now() - self._last_ack)) + " seconds ago"

        if self._wait_until < now():
            wa = "No"
        else:
            wa = str(int(self._wait_until - now())) + "s"

        if not self._gateway:
            ul = "Direct"
        elif self._gateway.is_available():
            ul = "Available"
        else:
            ul = "ROUTE OFFLINE"

        return """Node: {}
    id:              {}
    last_ack:        {}
    wait:            {}
    route to node    {}
    retransmissions: {}
    status:          {}""".format(self._name, self._node_id, la, wa, ul, self._rt_count, self._status)

    def __make_route_msg(self):
        routes = []

        # Add the gateway route
        if self._gateway is not None:
            routes = ["0:{}".format(self._gateway.node_id())]
        else:
            routes = ["0:0"]

        for dst, hop in self._config['routes']:
            if dst.startswith('#'):
                d = int(dst[1:])
            else:
                d = self._master.resolve_node(dst)
                d = d.node_id() if d is not None else 0
            if hop.startswith('#'):
                h = int(hop[1:])
            else:
                h = self._master.resolve_node(hop)
                h = h.node_id() if h is not None else 0
            routes += ["{}:{}".format(d, h)]
        routestr = ','.join(routes)

        return MessageCommand(self._node_id, "reset_routes", routestr)

    def __on_connected(self, code):
        if self._status["INITDONE"] and code == 3:
            self.log.info('reconnected to still configured node')
            self._status["REBUILD_SCH"] = True
        else:
            self.log.info('connected')
            self._status["REBUILD_SCH"] = False
            self._status["ROUTES"] = False
            self._status["INITDONE"] = False

        self._on_connected(code)

    def can_inject_command(self):
        return self._status['CON'] and self._status_on_ack is None

    def inject_command(self, cmd, args):
        if self.can_inject_command():
            self._injected_command = MessageCommand(self._node_id, cmd, args)

    def reset_timeout(self):
        self._wait_until = 0

    def _initialize(self):
        return None

    def _on_connected(self, code):
        pass

    def _next_message(self):
        return None

    def _on_connection_failed(self):
        pass

    def on_node_status_changed(self, node, status):
        pass
