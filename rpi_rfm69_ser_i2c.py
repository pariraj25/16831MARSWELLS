# Written by Alex Choi for 16.831 2026.03.24
# N.B. BLE, addressing and packet structues have NOT been implemented
# These will be covered in class

from smbus2 import SMBus, i2c_msg

import RPi.GPIO as GPIO
import time

RPI_I2C_BUS_NUM = 1
SERVER_I2C_ADDR = 0x43
I2C_START_REGIS = 0x00
RPI_RX_MSG_RECV = 4 # RPi BCM GPIO Pin 4 (Input)
BLE33_MAX_BYTES = 20
SMBUS_MAX_BYTES = 32

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
#------------------------
print("Waiting for message from Client...")

while True:
    while GPIO.input(RPI_RX_MSG_RECV) == GPIO.LOW:
        time.sleep(0.1)

    print("RF Message recevied from client!")
    msgFromClient = readDataI2C(SMBUS_MAX_BYTES)
    print(msgFromClient)

    # Send string
    #replyToClient = bytes("And To You Too!", "ascii") 
    
    # Send byte array
    replyToClient = bytearray([7, 8, 9, 10, 11, 12])
    bus.write_i2c_block_data(SERVER_I2C_ADDR, I2C_START_REGIS, replyToClient)


