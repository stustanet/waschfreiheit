#!/usr/bin/env python3
import wasch
import asyncio
import json
import sensor

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
            self.id = conf['id']
            self.name = conf['name']
            self.channels = { c['number']: WaschConfig.Node.Channel(c) for c in conf['channels'] }
            self.routes = conf['routes']
            self.routes_id = {}
            self.is_initialized = False

    def __init__(self, fname):
        with open(fname) as f:
            conf = json.load(f)

        try:
            self.nodes = { c['name']: WaschConfig.Node(c) for c in conf['nodes'] }
            # Postprocess the stuff
            for node in self.nodes.values():
                for rfrom, rto in node.routes.items():
                    node.routes_id[self.nodes[rfrom].id] = self.nodes[rto].id
        except KeyError:
            print("Config failed")
            raise


class NetworkManager:
    def __init__(self, config, wasch):
        self.config = config
        self.wasch = wasch
        self.nodes = {}

    async def reinit_node(self, node):
        node.is_initialized = False
        await self.init_the_network(node)

    async def init_the_network(self, node=None):
        if not node:
            node = self.config.nodes["MASTER"]

        print("Initing node ", node.name)
        if node.is_initialized:
            return

        if node.name == "MASTER":
            await self.wasch.sensor_routes(node.routes_id)
        else:
            connection = await self.wasch.node(node.id, self.config.nodes[node.routes["MASTER"]].id)
            await connection.routes(node.routes_id, reset=True)
            self.nodes[node.name] = connection
            node.is_initialized = True

        for n in node.routes.values():
            await self.init_the_network(self.config.nodes[n])

        # TODO Send sensor config data,
        # TODO enable the configured channels

async def run(conf, master):
    # TODO Here the magic happens:
    #
    # Magic auth-ping to keep testing the network to all nodes
    # reboot identification
    # Error handling for failed nodes:
    # 


    # Configure the master
    nm = NetworkManager(config, master)
    await nm.init_the_network()
    master.status_subscribe(lambda n, s: print("node:", n,"status", s))
    try:
        await nm.nodes["HSH7"].authping()
    except wasch.WaschError:
        print("Wasch has failed")


if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    # In order to test the interface open a virtual socket using
    # `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
    # And enter its /dev/pts/xx file here

    # if you have a real serial interface, just issue the serial /dev/ttyUSBX
    # here.
    config = WaschConfig('nodes.json')

    master = wasch.WaschInterface(
        "/dev/ttyUSB0", loop=loop,
        timeoutstrategy=wasch.TimeoutStrategy.NRetransmit(3) )

    loop.run_until_complete(run(config, master))

    loop.run_forever()
