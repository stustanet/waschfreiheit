#!/usr/bin/env python3

import serial
import socket
import hashlib
import sys

if len(sys.argv) != 3:
    print("USAGE: firmware_upgrade.py PORT IMAGE")
    print("PORT is the port to listen on. This port is the same as for the wasch controller.")
    print("IMAGE is the path to a binary image for the master node.")
    sys.exit(0)

with open(sys.argv[2], 'rb') as tmpfile:
    filedata = tmpfile.read()

hash_gen = hashlib.sha1()
hash_gen.update(filedata)
checksum = hash_gen.hexdigest()


print("New image has a size of {} bytes and the checksum {}.".format(len(filedata), checksum))

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

con = None

try:

    try:
        sock.bind(('0.0.0.0', int(sys.argv[1])))
    except OSError:
        print("Bind for port {} failed! Are you sure the wasch controller is NOT running?".format(sys.argv[1]))
        raise

    sock.listen(1)

    con, client = sock.accept()
    print("Got connection form ", client)

    con.sendall(b'flash_mcu_firmware\n')
    print("Update request sent, await response")
    print("Response:", con.recv(4096))

    print("Sending data")

    con.send('{}\n'.format(checksum).encode('ascii'))
    con.sendall('{}\n'.format(len(filedata)).encode('ascii'))
    con.sendall(filedata)

    while True:
        r = con.recv(4096)

        if not r:
            break

        print("", r)
finally:
    if con is not None:
        con.close()
    sock.close()
