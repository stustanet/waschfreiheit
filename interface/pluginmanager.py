import asyncio
import importlib
import collections

class PluginManager:
    class MasterNode:
        def __init__(*args):
            pass
        gateway = None
        routes = {}
        nodeid = 0


    def __init__(self, basepath, master, config):
        #basepath = Path(__file__).resolve().parent / "plugins"
        self.callables = []
        self.nodes = []
        print("[P] PluginManager starting.")
        print("[P] plugin basepath: {}".format(basepath))

        self.plugintypes = {"None":(None, None), "MASTER":(None, self.MasterNode)}
        self.master_routes = {}

        for filename in basepath.glob("*.py"):
            if (basepath / filename).exists():
                modname = "plugins." + filename.name.split('.')[0]
                print("[P] loading plugin: {}".format(modname))
                module = importlib.import_module(modname)
                self.setup_plugin(master, module, config)

        self.setup_network(config, master)
        del self.plugintypes

    def setup_plugin(self, master, module, config):
        """
        Setup and fix plugins
        """
        configname = '_'.join(module.__name__.split('.')[1:])
        print("[P]\tusing subconfig \"{}\"".format(configname))
        subconfig = config.subconfig(configname)
        plugin, plugintypes = module.init(master, subconfig)

        if plugin:
            if not getattr(plugin, "name", None):
                plugin.name = module.__name__
            self.callables.append(plugin)

        if plugintypes is None:
            # Skip nodes setup
            return

        if not isinstance(plugintypes, collections.Iterable):
            plugintypes = {plugintypes.__name__ : plugintypes}

        print("[P]\t{} a master plugin and defines {} types.".format(
            "defines" if plugin else "does not define",
            len(plugintypes)))

        self.plugintypes = {
            **self.plugintypes,
            **{tname: (subconfig, tclass)
               for tname, tclass in plugintypes.items()}
        }


    def setup_network(self, config, master):
        # Create a default master node - so that we can use it in the config
        nodes = {}

        # Setup the names and init the nodes
        from pprint import pprint; pprint(self.plugintypes)
        for nodename, networknodecfg in config['network'].items():
            nodecfg, NodeClass = self.plugintypes.get(networknodecfg['type'],
                                                      (None, None))
            if not NodeClass:
                raise TypeError("Unknwn Node type " + networknodecfg['type'])
            if NodeClass is None:
                continue
            if nodecfg:
                subconfig = nodecfg.subconfig(nodename)
            else:
                subconfig = None
            obj = (NodeClass(subconfig, master, networknodecfg['id'], nodename),
                   networknodecfg)
            nodes[nodename] = obj

        # Setup the routes and the gateways
        for nodename, a in nodes.items():
            node = a[0]
            networknodecfg = a[1]

            # Master as gateway means no gateway that can delay
            gateway = networknodecfg['gateway']
            if gateway in ["MASTER", "None"]:
                node.gateway = None
            else:
                node.gateway = nodes[gateway][0]

            node.routes = {
                nodes[dst][0].nodeid: nodes[hop][0].nodeid
                for dst, hop in networknodecfg['routes']
            }
            print("Done setting up node {nodename}: Gateway {gateway}, "
                  "routes: {node.routes}".format(
                nodename=nodename, node=node, gateway=gateway))

        # We clean the master from the list of nodes before it leaks out
        self.master_routes = nodes['MASTER'][0].routes
        del nodes['MASTER']
        self.nodes = [node for node, _ in nodes.values()]

    async def node_call(self, fname, required=False, **kwargs):
        await asyncio.gather(
            *[node.call(fname, required, **kwargs) for node in self.nodes])
        await self.call(fname, required, **kwargs)

    async def call(self, fname, required=False, **kwargs):
        """
        Call the given method on all plugins, proxying arguments
        """
        called = False
        result = {}
        for plugin in self.callables:
            func = getattr(plugin, fname, None)
            if func:
                if asyncio.iscoroutinefunction(func):
                    result[plugin.name] = await func(**kwargs)
                else:
                    result[plugin.name] = func(**kwargs)

    def sync_call(self, fname, required=False, **kwargs):
        """
        Call the given method on all plugins, proxying arguments
        """
        called = False
        result = {}
        for plugin in self.callables:
            func = getattr(plugin, fname, None)
            if func:
                if asyncio.iscoroutinefunction(func):
                    raise ArgumentError("Cannot call {} here - async context is"
                                        "required".format(fname))
                    #result[plugin.name] = loop.create_task(func(**kwargs))
                else:
                    result[plugin.name] = func(*args, **kwargs)
        pass
