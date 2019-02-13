"""
Implementation of the uplink to the status site
"""

import asyncio
import aiohttp
import urllib.parse


async def worker(queue):
    async with aiohttp.ClientSession() as session:
        while True:
            request = await queue.get()
            print("########## UPLINK REQUEST: ", request)
            try:
                async with session.get(request) as resp:
                    await resp.text()
            except (aiohttp.InvalidURL, aiohttp.ClientConnectorError) as exp:
                print(exp)


class WaschUplink:
    def __init__(self, config):
        self.config = config
        self.queue = asyncio.Queue()

        self.url = config['base_url']
        self.key = config['key']

        self.task = asyncio.ensure_future(worker(self.queue))

    def raw_request(self, request):
        self.queue.put_nowait(request)

    def on_serial_rx(self, data):
        data = urllib.parse.quote(data)
        request = "{}/extralog/RAW_RX/{}/{}".format(self.url, data, self.key)
        self.queue.put_nowait(request)

    def on_serial_tx(self, data):
        data = urllib.parse.quote(data)
        request = "{}/extralog/RAW_TX/{}/{}".format(self.url, data, self.key)
        self.queue.put_nowait(request)

    def send_alive_signal(self):
        request = "{}/lebt/{}".format(self.url, self.key)
        self.queue.put_nowait(request)

    def on_status_change(self, node, state):
        request = "{}/machine/{}/{}/{}".format(self.url, node, state, self.key)
        self.queue.put_nowait(request)
