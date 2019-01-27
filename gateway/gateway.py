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

cfgpath = 'waschstreamer.conf'


def serial_reader(ser, q, sig, cancel):
    try:
        while not cancel.is_set():
            data = ser.readline()
            if not data:
                continue
            q.put(data)

    except serial.SerialException:
        print("Serial error")
    sig.put(None)


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
    sig.put(None)


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

    sig.put(None)


def do_passthrough(ser, sock):
    sock.sendall(b'Starting passthrough mode\n')
    serial_queue = queue.Queue()
    sig_queue = queue.Queue()

    cancel = threading.Event()

    ser_rd = threading.Thread(target=serial_reader, args=(ser, serial_queue, sig_queue, cancel))
    ser_to_sock = threading.Thread(target=serial_to_sock, args=(serial_queue, sock, sig_queue, cancel))
    sock_to_ser = threading.Thread(target=sock_to_serial, args=(sock, ser, sig_queue, cancel))

    ser_rd.start()
    sock_to_ser.start()
    ser_to_sock.start()

    print("Wait for signal")
    sig_queue.get()

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

    # Sequence to enter bootloader
    wiringpi.digitalWrite(config['gpio_boot'], 0)
    wiringpi.digitalWrite(config['gpio_reset'], 1)
    time.sleep(0.1)
    wiringpi.digitalWrite(config['gpio_reset'], 0)
    time.sleep(0.2)
    wiringpi.digitalWrite(config['gpio_boot'], 1)

    run_stm_flasher(sf)

    sf.flush()



    time.sleep(0.2)

    # Finally reset the mcu again to start the new firmware
    wiringpi.digitalWrite(config['gpio_reset'], 1)
    time.sleep(0.2)
    wiringpi.digitalWrite(config['gpio_reset'], 0)


def handle_connection(ser, sock):
    while True:
        cmd = sock.makefile().readline()
        if not cmd:
            break

        cmd = cmd[:-1]

        if cmd == 'reset':
            wiringpi.digitalWrite(config['gpio_reset'], 1)
            time.sleep(0.2)
            wiringpi.digitalWrite(config['gpio_reset'], 0)
        elif cmd == 'forward':
            do_passthrough(ser, sock)
            return
        elif cmd == 'flash_mcu_firmware':
            ser.close()
            do_mcu_update(sock)
            return
        else:
            sock.sendall(b'Invalid command')
            return


def mainloop():
    while True:
        try:
            ser = serial.Serial(
                port=config['serial'],
                baudrate=int(config['baudrate']),
                timeout=1)

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

    # The boot pin needs to be high for normal operation
    wiringpi.digitalWrite(config['gpio_boot'], 1)
    # Set to output
    wiringpi.pinMode(config['gpio_boot'], 1)

    # The mcu is reset when this pin in high so it should normally low
    wiringpi.digitalWrite(config['gpio_reset'], 0)
    # Set to output
    wiringpi.pinMode(config['gpio_reset'], 1)


with open(cfgpath) as cfgf:
    config = libconf.load(cfgf)
init_gpio()
mainloop()
