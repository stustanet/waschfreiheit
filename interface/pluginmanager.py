import asyncio
import importlib


class PluginManager:
    def __init__(self, basepath, master, config):
        #basepath = Path(__file__).resolve().parent / "plugins"
        self.callables = []
        self.nodes = []
        print("[P] PluginManager starting.")
        print("[P] plugin basepath: {}".format(basepath))
        for filename in basepath.glob("*.py"):
            if (basepath / filename).exists():
                modname = "plugins." + filename.name.split('.')[0]
                print("[P] loading plugin: {}".format(modname))
                module = importlib.import_module(modname)
                self.setup_plugin(master, module, config)


    def setup_plugin(self, master, module, config):
        """
        Setup and fix plugins
        """
        configname = '_'.join(module.__name__.split('.')[1:])
        print("[P]\tusing subconfig \"{}\"".format(configname))
        plugin, pluginnodes = module.init(master, config.subconfig(configname))

        print("[P]\t{} a master plugin and defines {} sensors.".format(
            "defines" if plugin else "does not define",
            len(pluginnodes)))

        if plugin:
            if not getattr(plugin, "name", None):
                plugin.name = module.__name__
            self.callables.append(plugin)

        if pluginnodes:
            self.nodes += pluginnodes

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
