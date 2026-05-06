import serial
import struct
import time

FW_PATH = 'application_signed.bin'

with open(FW_PATH, 'rb') as f:
    firmware = f.read()

firmware = bytearray(firmware)
struct.pack_into('<I', firmware, 0, 1)
firmware_size = len(firmware)

ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

ser.writeTimeout = 0 
ser.isOpen() 

CHUNK = 256
sent = 0
while sent < firmware_size:
    chunk = firmware[sent:sent+CHUNK]
    ser.write(chunk)
    sent += len(chunk)
    print(f"Sent {sent}/{firmware_size} bytes")
    time.sleep(0.01)

print("\nDone")
ser.close()