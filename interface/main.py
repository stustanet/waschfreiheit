#!/usr/bin/env python3
import wasch
import machinemanager
import asyncio

async def observer(master):
    while True:
        await asyncio.sleep(1)
        if not master:
            continue
        state =  "*************************************************\n"
        state += "* Master:\n"
        state += "*  - response_pending: {}\n".format(master.response_pending)
        state += "*  - message_pending:  {}\n".format(master.message_pending)
        state += "*  - last_command:     {}\n".format(master.last_command)
        state += "*     - messagestr:    {}\n".format(master.last_command.message)
        state += "*     - retransmit:    {}\n".format(master.last_command.retransmit)
        state += "*     - r_count:       {}\n".format(master.last_command.retransmit_count)
        state += "*\n"
        state += "*  - Sensors [{}]:\n".format(len(master._sensors))
        for id, s in master._sensors.items():
            state += "*\n"
            state += "*    # SENSOR: ID {}: {}\n".format(s.nodeid, s.config.name)
            state += "*      - last_distance: {}\n".format(s.distance)
            state += "*      - last_status:   {}\n".format(s.last_status)
            state += "*      - last_update:   {}\n".format(s.last_update)
            state += "*      - state:         {}\n".format(s.state)
            if master.last_command.node == s:
                state += "*      - running command: {}\n".format(master.last_command)

        state +=  "\n\n"

        with open("/tmp/wasch.state", 'w+') as f:
            f.write(state)


async def network_sanity_observer(master):
    print("Starting network sanity observer every {} seconds".format(
        master.config.networkcheckintervall))

    while True:
        await asyncio.sleep(master.config.networkcheckintervall)
        try:
            print("Performing network sanity check")
            await master.networkmanager.recover_network(None)
        except wasch.WaschOperationInterrupted:
            print("Network sanity check was interrupted")
            pass

def node_status_update(node, status):
    print("\n\nNODE {} HAS NEW STATUS: {}\n\n".format(node, status))

async def run(master):
    # TODO Here the magic happens:
    #
    # Magic auth-ping to keep testing the network to all nodes
    # reboot identification
    # Error handling for failed nodes:

    # Configure the master
    master.status_subscribe(node_status_update)
    mm = machinemanager.MachineManager(master, "http://waschen.stusta.de", "token")

    try:
        await master.get_node("HSH16").authping()
    except wasch.WaschError:
        print("Wasch has failed")


if __name__ == "__main__":
    loop = asyncio.get_event_loop()

    # In order to test the interface open a virtual socket using
    # `socat -d -d pty,raw,echo=0 pty,raw,echo=0`
    # And enter its /dev/pts/xx file here

    # if you have a real serial interface, just issue the serial /dev/ttyUSBX
    # here.

    master = wasch.WaschInterface(
        "/dev/ttyUSB0", 'nodes.json', loop=loop)
        #"/dev/ttyUSB0",
        'nodes.json', loop=loop)

    observertask = loop.create_task(observer(master))

    try:
        loop.run_until_complete(master.start())
        loop.run_until_complete(run(master))
        networksanitizertask = loop.create_task(network_sanity_observer(master))
        loop.run_forever()
    except:
        raise
        pass
    finally:
        try:
            observertask.cancel()
            observertask.result()
        except (asyncio.CancelledError, asyncio.InvalidStateError):
            pass
        try:
            networksanitizertask.cancel()
            networksanitizertask.result()
        except asyncio.CancelledError:
            pass

    loop.cancel()
