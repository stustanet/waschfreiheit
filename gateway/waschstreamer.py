#!/usr/bin/env python3

import serial
import threading
import libconf
import socket
import hashlib
import queue
import socket
import time
import subprocess

# NOTE: We use wiringpi here instead of RPi.GPIO because the wiringpi lib allows
#       to set the pin value BEFORE configuring a pin as output.
import wiringpi

cfgpath = '/etc/waschstreamer/config.conf'

def reset_mcu(reset):
    if reset:
        # The mcu is reset when this pin is low
        wiringpi.digitalWrite(config['gpio_reset'], 0)
        # Set to output
        wiringpi.pinMode(config['gpio_reset'], 1)
    else:
        # In normal operation mode, we must not force the reset line to 3V3
        # Otherwise the software reset will no longer work!
        # So we just set the pin to input and rely on the pullup to keep the pin high.
        wiringpi.digitalWrite(config['gpio_reset'], 1)
        wiringpi.pinMode(config['gpio_reset'], 0)


def serial_reader(ser, q, sig, cancel):
    try:
        while not cancel.is_set():
            data = ser.readline()
            if not data:
                continue
            q.put(data)

    except serial.SerialException:
        print("Serial error")
    sig[0] = True


def serial_to_sock(q, sock, sig, cancel):
    try:
        while not cancel.is_set():
            data = q.get()
            if not data:
                continue
            print(data)
            sock.sendall(data)
    except socket.error:
        print("Socket error")
    sig[0] = True


def sock_to_serial(sock, ser, sig, cancel):
    sock.settimeout(1)

    try:
        while not cancel.is_set():
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue

            if not data:
                break

            print(data)
            ser.write(data)
    except serial.SerialException:
        print("Serial error")
    except socket.error:
        print("Socket error")

    sig[0] = True


def do_passthrough(ser, sock):
    sock.sendall(b'Starting passthrough mode\n')
    serial_queue = queue.Queue()
    sig = [False]

    cancel = threading.Event()

    ser_rd = threading.Thread(target=serial_reader, args=(ser, serial_queue, sig, cancel))
    ser_to_sock = threading.Thread(target=serial_to_sock, args=(serial_queue, sock, sig, cancel))
    sock_to_ser = threading.Thread(target=sock_to_serial, args=(sock, ser, sig, cancel))

    ser_rd.start()
    sock_to_ser.start()
    ser_to_sock.start()

    print("Wait for signal")
    while not sig[0]:
        time.sleep(1)
        # check to watchdog prefail signal
        if wiringpi.digitalRead(config['gpio_prefail']):
            print("WATCHDOG PREFAIL SIGNAL")
            try:
                # NOTE: This may f*** up the STATUS or ACK messages from the node,
                #       but a watchdog prefail is something that should NEVER happen!
                sock.sendall(b'===== WATCHDOG PREFAIL SIGNAL ====')
            except socket.error:
                print("Socket error")

    cancel.set()

    print("One threaded exited")

    serial_queue.put(None)

    ser_rd.join()
    sock_to_ser.join()
    ser_to_sock.join()

    print("xfer loop terminated")


def run_stm_flasher(sock):
    proc = subprocess.Popen(['python2', 'stm32loader/stm32loader.py', '-p', config['serial'], '-evw', '/tmp/firmware.bin'], stdout=subprocess.PIPE)
    for line in proc.stdout:
        sock.write(line)
        sock.flush()
    proc.wait()


def do_mcu_update(sock):
    # Receive image and store it into temporary file
    # The protocol for this is:
    #     The first line is the SHA1
    #     The second line is the image size in bytes as a decimal string.
    #     All following data is the new image in the binary format.
    # The received image is written into a tmp file.
    # After the expected number of bytes have been received, the checksum of the tmp file is generated.
    # If this checksum matches, the mcu is booted into the bootloader mode and the flasher util is invoked.

    print('Starting MCU update routine')

    sock.sendall(b'ACK Starting upgrade routine\n')

    sf = sock.makefile('rwb')

    checksum = sf.readline()
    if not checksum:
        return

    checksum = checksum[:-1]
    checksum = checksum.decode('ascii')

    imgsize = sf.readline()
    if not imgsize:
        return

    imgsize = imgsize[:-1]

    imgsize = imgsize.decode('ascii')
    imgsize = int(imgsize)

    if imgsize > config['max_update_size']:
        sf.write(b"ERR: Image too large!")
        sf.flush()
        return

    print("Expect image with {} bytes and checksum {}.".format(imgsize, checksum))

    received = 0
    with open("/tmp/firmware.bin", 'wb') as tmpfile:
        while received < imgsize:
            data = sf.read(imgsize - received)
            if not data:
                return
            received += len(data)
            tmpfile.write(data)

    hash_gen = hashlib.sha1()
    with open("/tmp/firmware.bin", 'rb') as tmpfile:
        hash_gen.update(tmpfile.read())

    if hash_gen.hexdigest() != checksum:
        sf.write(b"ERR Checksum mismatch")
        sf.flush()
        return


    print('Entering bootloader mode')

    # Sequence to enter bootloader
    wiringpi.digitalWrite(config['gpio_boot'], 1)
    reset_mcu(True)
    time.sleep(0.1)
    reset_mcu(False)
    time.sleep(0.2)
    wiringpi.digitalWrite(config['gpio_boot'], 0)

    print('Invoke flasher util')
    run_stm_flasher(sf)

    sf.flush()

    time.sleep(0.2)

    print('Reset MCU after flashing')

    # Finally reset the mcu again to start the new firmware
    reset_mcu(True)
    time.sleep(0.2)
    reset_mcu(False)


def handle_connection(ser, sock):
    while True:
        cmd = sock.makefile().readline()
        if not cmd:
            break

        cmd = cmd[:-1]

        print('Received command "{}"'.format(cmd))

        if cmd == 'reset':
            print('Reset MCU')
            reset_mcu(True)
            time.sleep(0.2)
            reset_mcu(False)
        elif cmd == 'forward':
            do_passthrough(ser, sock)
            return
        elif cmd == 'flash_mcu_firmware':
            ser.close()
            do_mcu_update(sock)
            return
        else:
            sock.sendall(b'Invalid command')
            print('Invalid command')
            return


def mainloop():
    while True:
        try:
            print("Opening serial port:", config['serial'])
            ser = serial.Serial(
                port=config['serial'],
                baudrate=int(config['baudrate']),
                timeout=1)

            print("Connect to {}:{}".format(config['host'], int(config['port'])))
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((config['host'], int(config['port'])))

            handle_connection(ser, sock)

            sock.close()
            ser.close()

        except serial.SerialException:
            print("Serial error on connect")
        except socket.error:
            print("Socket error on connect")

        time.sleep(1)


def init_gpio():
    wiringpi.wiringPiSetupGpio()

    # The boot pin needs to be low for normal operation
    wiringpi.digitalWrite(config['gpio_boot'], 0)
    # Set to output
    wiringpi.pinMode(config['gpio_boot'], 1)

    reset_mcu(False)

    # The 'prefail' input is set by the watchdog chip, if the watchdog reset is imminent
    # Set to input
    wiringpi.pinMode(config['gpio_prefail'], 0)


with open(cfgpath) as cfgf:
    config = libconf.load(cfgf)
init_gpio()
mainloop()
