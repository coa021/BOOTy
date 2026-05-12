import serial
import struct
import time
import zlib  # for crc32
import crcmod  # for crc16


def custom_crc32(data):
    crc = zlib.crc32(data) & 0xFFFFFFFF
    return struct.pack("<I", crc)


def custom_crc16(data):
    crc16_func = crcmod.mkCrcFun(0x18005, rev=True, initCrc=0xFFFF, xorOut=0x0000)
    crc = crc16_func(data)
    return struct.pack("<H", crc)  # 2bytes, little endian


ACK = 0x01
NACK = 0x15

with open("application_signed.bin", "rb") as f:
    firmware = f.read()

firmware = bytearray(firmware)
firmware_size = len(firmware)

ser = serial.Serial(
    port="/dev/ttyUSB2",
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=2,
)

print("tryint to send stuff")
sent = 0
BUFFER_SIZE = 256


# message = b"Hello world how are you?"
# crc = custom_crc16(message)

# payload = message + crc

# ser.write(payload)
# response = ser.read(1)
# if not response:
#     print("Timeout waiting for ACK")

# byte = response[0]
# if byte == ACK:
#     print("ACK")
#     sent += len(chunk)
#     print(f"Sent {sent} / {firmware_size}")
# elif byte == NACK:
#     print("NACK received")
while sent < firmware_size:
    chunk = bytes(firmware[sent : sent + BUFFER_SIZE])
    crc = custom_crc16(chunk)

    payload = bytes(chunk + crc)
    print(f"Trying to send following: {payload}\n\n")

    ser.write(payload)

    response = ser.read(1)
    if not response:
        print("Timeout waiting for ACK")
        break

    byte = response[0]
    if byte == ACK:
        print("ACK")
        sent += len(chunk)
        print(f"Sent {sent} / {firmware_size}")
    elif byte == NACK:
        print("NACK received")


time.sleep(1)

ser.close()
