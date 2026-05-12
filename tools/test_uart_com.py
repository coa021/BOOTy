import serial
import struct
import time
import zlib  # for crc32
import crcmod  # for crc16

ACK = 0x01
NACK = 0x15

ser = serial.Serial(
    port="/dev/ttyUSB2",
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
    timeout=2,
)


""" cases to test:
sent >256 chunk to see how the device responds, naturally this will fail and retry the same chunk again and again, after 10? tries i should stop i guess
send app without header
send <2 chunk to see how the device responds
send huge firmware size > flash sector

regarding app header tests: i have to check for invalid magic constant, version, fw size, crc32, signature. 

crc checks, change it 


"""
