# Written by Alex Choi for 16.831 2026.03.24
# N.B. BLE, addressing and packet structues have NOT been implemented
# These will be covered in class

from smbus2 import SMBus, i2c_msg

import RPi.GPIO as GPIO
import time
import os # file system operations (needed for image transfer later)
import struct # packing/unpacking numbers into bytes

RPI_I2C_BUS_NUM = 1
SERVER_I2C_ADDR = 0x43
I2C_START_REGIS = 0x00
RPI_RX_MSG_RECV = 4 # RPi BCM GPIO Pin 4 (Input)
BLE33_MAX_BYTES = 20
SMBUS_MAX_BYTES = 32
CHUNK_DATA_SIZE = 27
TIMEOUT_S = 2.0
CMD_TELEM_A = 0x01
CMD_TELEM_B = 0x02
CMD_IMG_REQUEST = 0x10
CMD_IMG_CONFIRM = 0x11
CMD_IMG_NACK = 0x13
CMD_IMG_ACK = 0x12
CMD_CHUNK_REQ = 0x14
CMD_CHUNK_DATA = 0x15
CMD_IMG_DONE = 0x16

# Setup Code ------------
bus = SMBus(RPI_I2C_BUS_NUM)
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RPI_RX_MSG_RECV, GPIO.IN, pull_up_down=GPIO.PUD_DOWN) # pull-down resistor (this is an active HI signal)
print("Server setup complete!")

# Helper functions ------
def readDataI2C(numBytes):
    read = i2c_msg.read(SERVER_I2C_ADDR, numBytes)
    bus.i2c_rdwr(read)
    return list(read)

def sendData(data):
    # Takes a bytearray or list of up to 32 bytes.
    # Creates a 32-byte bytearray filled with zeros.
    # Copies your data in, then calls the I2C write function.
    packet = bytearray(SMBUS_MAX_BYTES)       # zero-filled 32-byte buffer
    payload = bytes(data)                      # accept list or bytearray
    packet[:len(payload)] = payload            # copy data into front of buffer
    bus.write_i2c_block_data(SERVER_I2C_ADDR, I2C_START_REGIS, packet)
    return

def waitForMessage(timeout = TIMEOUT_S):
    # Polls the GPIO pin in a tight loop.
    # Records the start time so it can detect a timeout.
    # Returns True as soon as the pin goes HIGH (message ready).
    # Returns False if the elapsed time exceeds the timeout.
    start = time.time()
    while GPIO.input(RPI_RX_MSG_RECV) == GPIO.LOW:
        if (time.time() - start) >= timeout:
            return False
        time.sleep(0.001)               # ~1 ms poll interval to avoid hammering the bus
    return True

def buildTelemetryA():
    # Creates a 32-byte bytearray.
    # Byte 0: CMD_TELEM_A command identifier.
    # Bytes 1–4:   Temperature (°C)      — big-endian float
    # Bytes 5–8:   Battery voltage (V)   — big-endian float
    # Bytes 9–12:  Altitude (km)         — big-endian float
    packet = bytearray(SMBUS_MAX_BYTES)
    packet[0] = CMD_TELEM_A
    struct.pack_into('>f', packet, 1, 23.5)   # temperature  °C
    struct.pack_into('>f', packet, 5, 3.7)    # battery voltage V
    struct.pack_into('>f', packet, 9, 1.2)    # altitude km
    return packet

def buildTelemetryB():
    # Same structure as Telemetry A but carries attitude angles.
    # Byte 0: CMD_TELEM_B command identifier.
    # Bytes 1–4:   Roll  (°) — big-endian float
    # Bytes 5–8:   Pitch (°) — big-endian float
    # Bytes 9–12:  Yaw   (°) — big-endian float
    packet = bytearray(SMBUS_MAX_BYTES)
    packet[0] = CMD_TELEM_B
    struct.pack_into('>f', packet, 1, 15.0)   # roll  °
    struct.pack_into('>f', packet, 5, -3.2)   # pitch °
    struct.pack_into('>f', packet, 9, 270.0)  # yaw   °
    return packet

#------------------------
print("Waiting for message from Client...")

while True:
    # State 1: wait for a message signal on the GPIO pin
    if not waitForMessage():
        continue   # timed out — go back and wait again

    # State 2: read the incoming 32-byte command packet over I2C
    print("RF Message received from client!")
    msgFromClient = readDataI2C(SMBUS_MAX_BYTES)
    cmdByte = msgFromClient[0]   # byte 0 carries the command type

    # State 3: dispatch to the correct handler
    if cmdByte == CMD_TELEM_A:
        print("CMD_TELEM_A received — sending Telemetry A packet.")
        sendData(buildTelemetryA())

    elif cmdByte == CMD_TELEM_B:
        print("CMD_TELEM_B received — sending Telemetry B packet.")
        sendData(buildTelemetryB())

    else:
        print(f"WARNING: Unknown command byte received: 0x{cmdByte:02X}")
