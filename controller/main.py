#!/usr/bin/env python3
import asyncio
import time
import os
import logging
from pathlib import Path

from master import Master
from basenode import BaseNode
from waschnode import WaschNode
from manhattannode import ManhattanNode
from uplink import WaschUplink
from debuginterface import DebugInterface

from configuration import Configuration


async def statuswriter(master):
    while True:
        await asyncio.sleep(1)

        if not master.initialized:
            continue

        state = "*************************************************\n"
        state += "Update time: {}\n".format(time.asctime())
        state += master.debug_state()

        with open("/tmp/wasch.state", 'w+') as statefile:
            statefile.write(state)



def load_nodes(config, master, uplink):
    nc = config.subconfig('network')

    for n in config['network']:
        if n == 'MASTER':
            continue

        # Node will register itself at the master
        t = nc[n]['type']

        if t == 'base':
            BaseNode(nc.subconfig(n), master, n)
        elif t == 'wasch':
            WaschNode(nc.subconfig(n), master, n, uplink)
        elif t == 'manhattan':
            ManhattanNode(nc.subconfig(n), master, n, uplink)
        else:
            raise KeyError("Unknown node type", t)


def main(configprefix):
    logformat = '%(asctime)s | %(name)s | %(levelname)5s | %(message)s'
    logging.basicConfig(format=logformat, filename='/var/log/wasch/activity.log', level=logging.DEBUG)
    log = logging.getLogger('WiF')
    log.info("starting wasch interface")

    loop = asyncio.get_event_loop()

    config = Configuration(configfile='config.conf', pathprefix=configprefix)

    uplink = WaschUplink(config.subconfig('uplink'))

    master = Master(loop,
                    config.subconfig('network').subconfig('MASTER'),
                    uplink)


    load_nodes(config, master, uplink)

    di = DebugInterface(master)

    state_task = asyncio.ensure_future(statuswriter(master))

    #pluginpath = Path(__file__).resolve().parent / "plugins"

    #pluginmanager = PluginManager(pluginpath,
    #                              master,
    #                              masterconfig)
    #master.pluginmanager = pluginmanager

    loop.run_until_complete(master.run())

if __name__ == "__main__":
    main("conf")
