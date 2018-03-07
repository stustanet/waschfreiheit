import aiohttp
import asyncio
import base64

class WaschUplink:
    def __init__(self, base_url, auth, loop=None):
        if not loop:
            loop = asyncio.get_event_loop()
        self.loop = loop
        self.base_url = base_url
        self.__key = auth


    def machine_name(self, node):
        # machinenname hat die form HSH16, HSH10,...
        return node.config.name

    async def status_update(self, node, status):
        machine = self.machine_name(node)
        url = "{}/machine/{}/{}/{}".format(self.base_url, machine, status, self.__key)
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url) as resp:
                    await resp.text()
        except (aiohttp.InvalidURL, aiohttp.ClientConnectorError) as exp:
            print(exp)

    async def statistics_update(self, node, status):
        machine = self.machine_name(node)
        url = "{}/extralog/{}/{}/{}".format(self.base_url, machine, status,
                                            self.__key)
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url) as resp:
                    await resp.text()
        except (aiohttp.InvalidURL, aiohttp.ClientConnectorError) as exp:
            print(exp)


    async def heartbeat(self):
        url = "{}/lebt/{}".format(self.base_url, self.__key)
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url) as resp:
                    await resp.text()
        except (aiohttp.InvalidURL, aiohttp.ClientConnectorError) as exp:
            print(exp)
