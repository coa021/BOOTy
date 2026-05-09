import serial
import struct
import time
import zlib  # for crc32
import crc16 # crc 16

def custom_crc32(data):
    crc = zlib.crc32(data) & 0xFFFFFFFF
    return struct.pack("<I", crc)

def custom_crc16(data):
    return crc16.crc16xmodem(data)


ACK = 0x01
NACK = 0x15

with open("application_signed.bin", "rb") as f:
    firmware = f.read()

firmware = bytearray(firmware)
firmware_size = len(firmware)

ser = serial.Serial(
    port="/dev/ttyUSB0",
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=2,
)


sent = 0
print("tryint to send stuff")
while sent < firmware_size:
    chunk = bytes(firmware[sent : sent + 256])
    # crc = custom_crc16(chunk)

    payload = bytes(chunk)
    ser.write(payload)

    response = ser.read(1)
    if not response:
        print("Timeout waiting for ACK")

    byte = response[0]
    if byte == ACK:
        print("ACK")
        sent += len(payload)
        print(f"Sent {sent} / {firmware_size}")
        time.sleep(0.5)
    elif byte == NACK:
        print("NACK received")
        continue



time.sleep(1)

ser.close()
