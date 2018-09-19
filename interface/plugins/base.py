def init(master, config):
    return BasePlugin(master, config), []

class BasePlugin:
    def __init__(self, master, config):
        self.master = master
        self.config = config

    async def on_serial_available(self):

        # first generate the routes
        routes = self.master_routes()
        msg = ",".join(["{}:{}".format(dst, hop) for dst, hop in routes])

        await self.master.send("routes " + msg + "\n")

    async def on_serial_error(self, error):
        print("Serial error occurred: ", error)

    async def on_read_errot(self, error):
        print("Read error occurred: ", error)

    def master_routes(self):
        return [(0,0)]
        pass
