# Written by Alex Choi for 16.831 2026.03.24
# Addressing, packetization and error correction are NOT implemented
# They will be explained during class

from smbus2 import SMBus, i2c_msg

import RPi.GPIO as GPIO
import time
import os # file system operations (needed for image transfer later)
import struct # packing/unpacking numbers into bytes


RPI_I2C_BUS_NUM = 1
CLIENT_I2C_ADDR = 0x42
I2C_START_REGIS = 0x00
RPI_RX_MSG_RECV = 4  # RPi Board GPIO pin 4 (Input)
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

# Setup code -------------------
bus = SMBus(RPI_I2C_BUS_NUM)
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(RPI_RX_MSG_RECV, GPIO.IN, pull_up_down= GPIO.PUD_DOWN) # pull-up resistor (this is an active HI signal)
print("Client setup complete!")

# Helper functions -------------
def readDataI2C(numBytes):
    read = i2c_msg.read(CLIENT_I2C_ADDR, numBytes)
    bus.i2c_rdwr(read)
    return list(read)

def sendData(data):
    # Takes a bytearray or list of up to 32 bytes.
    # Creates a 32-byte bytearray filled with zeros.
    # Copies your data in, then calls the I2C write function.
    packet = bytearray(SMBUS_MAX_BYTES)       # zero-filled 32-byte buffer
    payload = bytes(data)                      # accept list or bytearray
    packet[:len(payload)] = payload            # copy data into front of buffer
    bus.write_i2c_block_data(CLIENT_I2C_ADDR, I2C_START_REGIS, packet)
    return

def waitForMessage(timeout = TIMEOUT_S):
    # Polls the GPIO pin in a tight loop.
    # Records the start time so it can detect a timeout.
    # Returns True as soon as the pin goes HIGH (message ready).
    # Returns False if the elapsed time exceeds the timeout.
    # This replaces the bare while-loop in the original scripts.
    start = time.time()
    while GPIO.input(RPI_RX_MSG_RECV) == GPIO.LOW:
        if (time.time() - start) >= timeout:
            return False
        time.sleep(0.001)               # ~1 ms poll interval to avoid hammering the bus
    return True

# other helpers
def buildTelemPacket(cmd):
    """Return a 32-byte bytearray whose first byte is cmd (CMD_TELEM_A or CMD_TELEM_B)."""
    packet = bytearray(SMBUS_MAX_BYTES)
    packet[0] = cmd
    return packet

def parseTelemResponse(response):
    """
    Parse three little-endian floats packed at bytes 1-4, 5-8, and 9-12
    of the 32-byte response list/bytearray.
    Returns (float0, float1, float2).
    """
    raw = bytes(response)
    f0 = struct.unpack_from('<f', raw, 1)[0]    # bytes 1-4
    f1 = struct.unpack_from('<f', raw, 5)[0]    # bytes 5-8
    f2 = struct.unpack_from('<f', raw, 9)[0]    # bytes 9-12
    return f0, f1, f2

# ------------------------------

# print("Sending message to server...")

# # Send string
# #dataToSend = bytes("Hello World", "ascii")
# #bus.write_i2c_block_data(CLIENT_I2C_ADDR, I2C_START_REGIS, dataToSend)

# # Send byte array
# dataToSend = bytearray([1, 2, 3, 4, 5, 6])
# bus.write_i2c_block_data(CLIENT_I2C_ADDR, I2C_START_REGIS, dataToSend)

# while GPIO.input(RPI_RX_MSG_RECV) == GPIO.LOW: # Block until message received
#     # Wait for a message using your new waitForMessage() function.
#     # Read 32 bytes over I2C.
#     # Read byte 0 as the command type.
#     # Use if/elif/else to handle each command type:
#         # If CMD_TELEM_A: call your telemetry A builder and send the result.
#         # If CMD_TELEM_B: call your telemetry B builder and send the result.
#         # Else: print an unknown command warning
#     time.sleep(0.1)
    
# # Replace the one-shot client script with an interactive loop:
#     # • Print a simple command menu: t = Telemetry A, u = Telemetry B, i = Image, q = Quit.
#     # • Use input() to read a keypress.
#     # • For t and u: build a 32-byte packet with the command byte set, send it, wait for a response, parse and print the three floats.
#     # • For parsing: use struct to read each float from the response at the correct byte offset.

# print("RF Message received from server!")
# msgFromServer = readDataI2C(SMBUS_MAX_BYTES)
# print(msgFromServer)



# ── Reactive server-command loop ──────────────────────────────────────────────
# Sits here until the server stops asserting commands on the GPIO line,
# then falls through to the interactive client menu below.
print("Waiting for server commands…")
while True:
    if not waitForMessage(timeout=TIMEOUT_S):
        # No message arrived within the timeout – assume server is done
        print("No server command pending, entering interactive mode.")
        break

    response = readDataI2C(SMBUS_MAX_BYTES)
    cmd = response[0]

    if cmd == CMD_TELEM_A:
        print("Server requested Telemetry A")
        packet = buildTelemPacket(CMD_TELEM_A)
        sendData(packet)

    elif cmd == CMD_TELEM_B:
        print("Server requested Telemetry B")
        packet = buildTelemPacket(CMD_TELEM_B)
        sendData(packet)

    else:
        print(f"Unknown command received: 0x{cmd:02X}")

# ── Interactive client loop ───────────────────────────────────────────────────
MENU = """
Commands:
  t – Request Telemetry A
  u – Request Telemetry B
  i – Request Image
  q – Quit
"""

while True:
    print(MENU)
    key = input("Enter command: ").strip().lower()

    # ── Telemetry A ──────────────────────────────────────────────────────────
    if key == 't':
        sendData(buildTelemPacket(CMD_TELEM_A))
        print("Sent CMD_TELEM_A, waiting for response…")
        if not waitForMessage():
            print("  ✗ Timeout waiting for Telemetry A response")
            continue
        raw = readDataI2C(SMBUS_MAX_BYTES)
        f0, f1, f2 = parseTelemResponse(raw)
        print(f"  Telemetry A → val0={f0:.4f}  val1={f1:.4f}  val2={f2:.4f}")

    # ── Telemetry B ──────────────────────────────────────────────────────────
    elif key == 'u':
        sendData(buildTelemPacket(CMD_TELEM_B))
        print("Sent CMD_TELEM_B, waiting for response…")
        if not waitForMessage():
            print("  ✗ Timeout waiting for Telemetry B response")
            continue
        raw = readDataI2C(SMBUS_MAX_BYTES)
        f0, f1, f2 = parseTelemResponse(raw)
        print(f"  Telemetry B → val0={f0:.4f}  val1={f1:.4f}  val2={f2:.4f}")

    # ── Image transfer ───────────────────────────────────────────────────────
    elif key == 'i':
        # Phase 1: request
        packet = bytearray(SMBUS_MAX_BYTES)
        packet[0] = CMD_IMG_REQUEST
        sendData(packet)
        print("Sent CMD_IMG_REQUEST, waiting for confirm…")

        if not waitForMessage():
            print("  ✗ Timeout waiting for image confirm")
            continue

        raw = readDataI2C(SMBUS_MAX_BYTES)
        if raw[0] != CMD_IMG_CONFIRM:
            print(f"  ✗ Expected CMD_IMG_CONFIRM (0x{CMD_IMG_CONFIRM:02X}), got 0x{raw[0]:02X}")
            nack = bytearray(SMBUS_MAX_BYTES)
            nack[0] = CMD_IMG_NACK
            sendData(nack)
            continue

        # Bytes 1-4: total image size in bytes (little-endian uint32)
        total_bytes = struct.unpack_from('<I', bytes(raw), 1)[0]
        num_chunks  = (total_bytes + CHUNK_DATA_SIZE - 1) // CHUNK_DATA_SIZE
        print(f"  Image size: {total_bytes} bytes → {num_chunks} chunks")

        # Phase 2: ACK and receive chunks
        ack = bytearray(SMBUS_MAX_BYTES)
        ack[0] = CMD_IMG_ACK
        sendData(ack)

        image_data = bytearray()
        for chunk_idx in range(num_chunks):
            # Request chunk
            req = bytearray(SMBUS_MAX_BYTES)
            req[0] = CMD_CHUNK_REQ
            struct.pack_into('<H', req, 1, chunk_idx)   # 2-byte chunk index
            sendData(req)

            if not waitForMessage():
                print(f"  ✗ Timeout on chunk {chunk_idx}")
                break

            chunk_raw = readDataI2C(SMBUS_MAX_BYTES)
            if chunk_raw[0] != CMD_CHUNK_DATA:
                print(f"  ✗ Unexpected response 0x{chunk_raw[0]:02X} for chunk {chunk_idx}")
                break

            # Byte 3 = number of valid data bytes in this chunk
            chunk_len  = chunk_raw[3]
            chunk_data = chunk_raw[4: 4 + chunk_len]
            image_data.extend(chunk_data)
            print(f"  Chunk {chunk_idx+1}/{num_chunks} received ({chunk_len} bytes)", end='\r')

        print()   # newline after progress

        # Phase 3: signal done
        done = bytearray(SMBUS_MAX_BYTES)
        done[0] = CMD_IMG_DONE
        sendData(done)

        # Save to disk
        filename = f"received_image_{int(time.time())}.bin"
        with open(filename, 'wb') as f:
            f.write(image_data)
        print(f"  ✓ Image saved to {filename} ({len(image_data)} bytes)")

    # ── Quit ─────────────────────────────────────────────────────────────────
    elif key == 'q':
        print("Exiting.")
        break

    else:
        print("  Unrecognised command, please try again.")

GPIO.cleanup()
bus.close()