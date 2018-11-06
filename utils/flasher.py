#!/bin/env python3

'''
Flasher util for the sensor node.
This way of flashing is unsafe so before using this so when using this you should have a backup plan.
'''

import serial
import sys
import time

baudrate_config = 115200
baudrate_flash = 115200
imagesize = 1024 * 63

# Number of 16-bit chunks to write at once
blocksize = 128

def print_char(f, buf, idx):
    f.write(buf[idx:idx + 1])

def write_flash(s, buf):
    # Start write mode
    s.write(b'w')
    cycles = int(len(buf) / (blocksize * 2))
    for i in range(cycles):
        # Wait for the next chunk sign
        char = s.read()
        if not char == b'.':
            print('flash failed: ', char.decode('latin1'))
            return False
        for j in range(blocksize):
            print_char(s, buf, i * blocksize * 2 + j * 2)
            print_char(s, buf, i * blocksize * 2 + j * 2 + 1)
        print("\r%d%%" % int(i * 100 / cycles), end='')

    print("\r100%")

    # Done -> check finished sign
    char = s.read()
    if not char == b'W':
        print('flash not finished: ', char.decode('latin1'))
        return False

    return True

def verify_flash(s, buf):
    # Start verify mode
    s.write(b'v')
    cycles = int(len(buf) / (blocksize * 2))
    for i in range(cycles):
        # Wait for the next chunk sign
        char = s.read()
        if not char == b'.':
            print('verify failed: ', char.decode('latin1'))
            return False
        for j in range(blocksize):
            print_char(s, buf, i * blocksize * 2 + j * 2)
            print_char(s, buf, i * blocksize * 2 + j * 2 + 1)
        print("\r%d%%" % int(i * 100 / cycles), end='')

    print("\r100%")

    # Done -> check finished sign
    char = s.read()
    if not char == b'V':
        print('Verfy not finished: ', char.decode('latin1'))
        return False

    return True

def erase_flash(s):
    s.write(b'e')
    char = s.read()
    if not char == b'E':
        print('Erase error: ', char.decode('latin1'))
        return False
    return True

def exit_flasher(s):
    s.write(b'x')
    char = s.read()
    if not char == b'X':
        print('Unexpected answer: ', char.decode('latin1'))
        return False
    return True

def flush_con(s):
    # Send 0xff until the result reads ~
    while (s.in_waiting() == 0 or s.read() != b'~'):
        s.write(b'\xff')

    # Remove remaining bytes from buffer
    while (s.in_waiting() != 0):
        s.read()

if len(sys.argv) != 3:
    print("USAGE: flasher.py SERIAL IMAGE")
    sys.exit(1)

s = serial.Serial(sys.argv[1], baudrate_config)
with open(sys.argv[2], 'rb') as f: 
    data = f.read()

if len(data) > imagesize:
    print ("Image too large!")
    sys.exit(1)

print("Writing image with size: ", len(data))

# Pad data to fill whole flash
data += b'\xff' * (imagesize - len(data))

print("Flash size: ", len(data))

print('Serial and file open -> send flash command')
# Init flash mode
s.write(b'\nfirmware_upgrade --start %d\n' % baudrate_flash)

s.flush()

# Wait for the command to be sent
time.sleep(0.2)


print("Set programming baudrate")
s.baudrate = baudrate_flash

s.flush()

# wait for the '$' sign, this indicates the the MCU is waiting for the unlock code
print('Wait for the start sign')
while True:
    char = s.read()
    if char == b'$':
        break
    print(char.decode('latin1'), end='')

print('OK, sending unlock code')
s.write(b'ABC')
char = s.read()
if not char == b'U':
    print('Unlock code rejected: ', char.decode('latin1'));
    sys.exit(1)

print('Code accepted')

while (True):
    print('Erasing flash')
    if not erase_flash(s):
        print('Retry')
        flush_con(s)
        continue

    print('Erase OK -> write data')
    if not write_flash(s, data):
        print ("Flash failed -> retry")
        flush_con(s)
        continue

    print('Write OK -> verify data')
    if not verify_flash(s, data):
        print ("Verify failed -> retry")
        flush_con(s)
        continue

    # Everything OK
    print('Verify OK -> exit the flasher')
    exit_flasher(s)
    break


print("Yay, we flashed a new image!!")
