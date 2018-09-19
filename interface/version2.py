import asyncio
from pathlib import Path

from master import Master
from pluginmanager import PluginManager
from configuration import Configuration

def main(configfile):
    loop = asyncio.get_event_loop()

    masterconfig = Configuration(configfile=configfile)

    master = Master(loop,
                    masterconfig['serial']['device'],
                    masterconfig['serial']['baudrate'])
    pluginpath = Path(__file__).resolve().parent / "plugins"

    pluginmanager = PluginManager(pluginpath,
                                  master,
                                  masterconfig)
    master.pluginmanager = pluginmanager

    loop.run_until_complete(master.run())

if __name__ == "__main__":
    main("./v2config.conf")
