import serial
import struct
import time
import zlib  # for crc

def custom_crc(data):
    crc = zlib.crc32(data) & 0xFFFFFFFF
    return struct.pack("<I", crc)


def build_packet(cmd, payload):
    length = struct.pack(
        "<H", len(payload)
    )  # this is little endian(<), unsigned short(H) as i understoor
    header = bytes([cmd]) + length
    body = header + payload
    crc = custom_crc(body)
    return body + crc


def wait_for_ack(ser, timeout=2.0):
    ser.timeout = timeout
    response = ser.read(1)
    if not response:
        print("Timeout waiting for ACK")
        return False
    byte = response[0]
    if byte == ACK:
        return True
    elif byte == NACK:
        print("NACK received")
        return False
    else:
        print(f"Unknown response: 0x{byte:02X}")
        return False


def send_packet_with_retry(ser, cmd, payload, retries=3):
    packet = build_packet(cmd, payload)
    for attempt in range(retries):
        ser.write(packet)
        if wait_for_ack(ser):
            return True
        print(f"Retry {attempt +1}/{retries}")
    return False


FW_PATH = "application_signed.bin"
CHUNK_SIZE = 256
ACK = 0x06
NACK = 0x15

CMD_START = 0x01
CMD_DATA = 0x02
CMD_END = 0x03

with open(FW_PATH, "rb") as f:
    firmware = f.read()

firmware = bytearray(firmware)
firmware_size = len(firmware)

ser = serial.Serial(
    port="/dev/ttyUSB1",
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
)

ser.writeTimeout = 0
ser.isOpen()


# sending the fw size
size_payload = struct.pack("<I", firmware_size)
if not send_packet_with_retry(ser, CMD_START, size_payload):
    print("Failed to send fw size payload")
    ser.close()
    exit(1)

# sending the data
sent = 0
while sent < firmware_size:
    chunk = bytes(firmware[sent : sent + CHUNK_SIZE])
    if not send_packet_with_retry(ser, CMD_DATA, chunk):
        print(f"Failed sending packet even with retries: {sent}")
        ser.close()
        exit(1)
    sent += len(chunk)
    print(f"Sent {sent}/{firmware_size} bytes", sep="\r")
    print(f"\t{chunk}")

# im sending the end of communication command
if not send_packet_with_retry(ser, CMD_END, b""):
    print("Failed to send END cmd")
    ser.close()
    exit(1)

print("Done")
ser.close()
