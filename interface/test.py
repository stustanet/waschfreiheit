import asyncio
import os
import re
import sys
import random
import logging

from asyncio.streams import StreamWriter, FlowControlMixin

# Setup the log
logformat = '%(asctime)s | %(name)s | %(levelname)s | %(message)s'
logging.basicConfig(format=logformat)
log = logging.getLogger('we')
log.setLevel(logging.DEBUG)
log.info("starting waschemulator")

async def normal_op_test(stdin, stdout, loop=None):
    while True:
        await setup(stdin, stdout, loop)
        await asyncio.sleep(0.1)
        for i in range(10):
            global nodes
            node, _ = random.choice(list(nodes.items()))
            status = random.randint(0, 3)
            await command(stdout, "###STATUS{} {}".format(node, status))


async def one_node_dead(stdin, stdout, loop=None):
    while True:
        await setup(stdin, stdout, loop, dead_nodes=['2'])

        for i in range(10):
            global nodes
            node, _ = random.choice(list(nodes.items()))
            status = random.randint(0, 3)
            await command(stdout, "###STATUS{} {}".format(node, status))


async def node_irresponsive(stdin, stdout, loop=None):
    while True:
        await setup(stdin, stdout, loop, random_dead_nodes=['1', '2', '3', '4'])
        for i in range(10):
            global nodes
            node, _ = random.choice(list(nodes.items()))
            status = random.randint(0, 3)
            await command(stdout, "###STATUS{} {}".format(node, status))


nodes = {}
async def setup(stdin, stdout, loop=None, dead_nodes=[], random_dead_nodes=[]):
    async for line in stdin:
        line = line.decode('ascii')
        log.info("I: %s", line.strip())

        if line[0:7] == 'routes ':
            continue


        match = re.findall(r'(\d+)', line)
        if match:
            if match[0] in dead_nodes:
                await command(stdout, "###TIMEOUT{}".format(match[0]))
            elif match[0] in random_dead_nodes and random.random() < 0.7:
                await command(stdout, "###TIMEOUT{}".format(match[0]))
            else:
                # Now in match[0] we have the node to ACK:
                await command(stdout, "###ACK{}-0".format(match[0]))
                global nodes
                nodes[int(match[0])] = True
        else:
            print("Unknown line", line, file=sys.stderr)

        if line.strip() == "authping 4":
            break

    # Now we can penetrate the network!
    log.info("We have finished a normal network setup.")


async def command(stdout, cmd):
    if cmd[-1] != '\n':
        cmd += '\n'
    stdout.write(cmd.encode('ascii'))
    await stdout.drain()
    log.info("O: %s", cmd.strip())

async def setup_stdio(loop=None):
    if not loop:
        loop = asyncio.get_event_loop()
    reader = asyncio.StreamReader()
    reader_protocol = asyncio.StreamReaderProtocol(reader)

    writer_transport, writer_protocol = await loop.connect_write_pipe(FlowControlMixin, os.fdopen(1, 'wb'))
    writer = StreamWriter(writer_transport, writer_protocol, None, loop)

    await loop.connect_read_pipe(lambda: reader_protocol, sys.stdin)
    return reader, writer

async def send(command):
    if command [-1] != '\n':
        command += '\n'
    await stdout.write(command.encode('ascii'))

def test(mode):
    loop = asyncio.get_event_loop()
    stdin, stdout = loop.run_until_complete(setup_stdio(loop=loop))


    modes = {
        "normal":normal_op_test,
        "one_dead":one_node_dead,
        "irresponsive":node_irresponsive
    }

    if mode in modes:
        loop.run_until_complete(modes[mode](stdin, stdout, loop=loop))
    else:
        raise KeyError("Invalid Mode")

if __name__== "__main__":
    test(sys.argv[1])
