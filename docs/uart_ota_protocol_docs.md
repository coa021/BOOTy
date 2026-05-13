# OTA UART Protocol — STM32F411 Black Pill

**Protocol class:** Stop-and-Wait ARQ (Automatic Repeat reQuest)  
**Transport:** UART @ 115200 8N1  
**Source:** `send_update.py` → `application_signed.bin` → STM32F411

---

## 1. Packet Structure

Every packet sent over UART has this exact binary layout — **263 bytes total**:

```
 offset  size   field            type        description
 ------  ----   -----            ----        -----------
 0       4      package_counter  uint32_t    1-based counter, increments on each ACK. Little-Endian.
 4       1      flag             uint8_t     0x00 = NORMAL_PACKAGE, 0x01 = LAST_PACKAGE
 5       256    chunk            uint8_t[]   Raw slice of the firmware binary
 261     2      crc16            uint16_t    CRC-16/IBM over chunk only. Little-Endian.
                                             Total: 4+1+256+2 = 263 bytes
```

**Python construction:**
```python
header  = struct.pack("<IB", package_counter, flag)   # 5 bytes, LE
chunk   = bytes(firmware[sent : sent + 256])           # 256 bytes
crc     = custom_crc16(chunk)                          # 2 bytes, LE
payload = header + chunk + crc                         # 263 bytes
```

> **Note:** CRC is computed over the **chunk only** (bytes 5–260), not over the header.  
> Reference: [struct.pack format strings](https://docs.python.org/3/library/struct.html#format-characters)

---

## 2. CRC-16 Parameters

The `crcmod.mkCrcFun` call uses the following parameters:

| Parameter | Value         | Meaning                          |
|-----------|---------------|----------------------------------|
| poly      | `0x18005`     | CRC-16/IBM polynomial (0x8005 with leading 1-bit) |
| rev       | `True`        | Reflected input (LSB-first)      |
| initCrc   | `0xFFFF`      | Initial register value           |
| xorOut    | `0x0000`      | No final XOR                     |

This is the **CRC-16/IBM** (also called CRC-16/ARC or CRC-16/LHA) variant.  
Reference: [CRC catalogue — CRC-16/ARC](https://reveng.sourceforge.io/crc-catalogue/16.htm#crc.cat.crc-16-arc)

The MCU must use identical parameters. In C with STM32 HAL or a software implementation:

```c
/* poly=0x8005, refIn=true, refOut=true, init=0xFFFF, xorOut=0x0000 */
uint16_t crc16_ibm(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;  /* reflected poly */
            else
                crc >>= 1;
        }
    }
    return crc;
}
```

> 0xA001 is the bit-reflection of 0x8005.  
> Reference: [Williams, R. "A Painless Guide to CRC Error Detection Algorithms"](http://www.ross.net/crc/download/crc_v3.txt)

---

## 3. Flag Field

| Value  | Constant            | Meaning                                    |
|--------|---------------------|--------------------------------------------|
| `0x00` | `NORMAL_PACKAGE_FLAG` | More packets follow                      |
| `0x01` | `LAST_PACKAGE_FLAG`   | This is the final chunk; transfer ends   |

The flag is set by:
```python
flag = LAST_PACKAGE_FLAG if (sent + BUFFER_SIZE >= firmware_size) else NORMAL_PACKAGE_FLAG
```

The MCU uses this to know when to stop buffering and begin verification/flash swap.

---

## 4. Handshake — Stop-and-Wait ARQ

This is a **stop-and-wait** protocol: the host sends exactly one packet, then **blocks** waiting for a 1-byte response before sending the next.

```
Host                          MCU
 |                             |
 |--- packet N (263 bytes) --->|
 |                             |  validate CRC16(chunk)
 |                             |  write chunk to flash
 |<---- 0x01 (ACK) -----------|  if CRC OK
 |                             |
 | sent += 256                 |
 | package_counter += 1        |
 |                             |
 |--- packet N+1 (263 bytes) ->|
 |                             |
```

**On NACK (0x15):** the host currently prints the error and does **not** retry — no retransmission logic is implemented. The loop continues to the next `ser.read()` call, which will timeout.

**On timeout (4 seconds, no response):** the host prints "Timeout" and `break`s the loop. No reconnect.

ACK/NACK values:
```python
ACK  = 0x01
NACK = 0x15   # ASCII NAK character
```

Reference: [PySerial documentation](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)

---

## 5. Byte-level Packet Example

**Packet #1, normal (not last):**

```
Byte index:   0    1    2    3    4    5  6  7  ...  260   261  262
              ---- ---- ---- ---- ---- ------------------------- ---- ----
Value (hex):  01   00   00   00   00   D0 D1 D2 ...  D255  CL   CH
              |____________| |__| |_________________________| |______|
               counter=1(LE) flag=0  256 bytes of firmware    CRC16 LE
```

**Packet #N, last:**

```
Byte index:   0    1    2    3    4    5  ...  260   261  262
Value (hex):  NN   NN   NN   NN   01   D0 ...  D255  CL   CH
                             ^^^^ flag=0x01  ← LAST_PACKAGE_FLAG
```

If the firmware size is not a multiple of 256, Python's slice `firmware[sent : sent + 256]` will return fewer than 256 bytes for the final chunk. The `len(chunk)` will be < 256, and `crc` is computed over that shorter slice. The `total` logged will be less than 263.

---

## 6. Application Header

The binary file (`application_signed.bin`) that the host sends starts with a **512-byte block** that contains the application header struct followed by padding.

### 6.1 Struct layout

```c
/* app_header.h */
/* 80 bytes in total */
struct app_header_t {
    uint32_t magic;         /* offset  0 — magic constant, e.g. 0xDEADBEEF */
    uint32_t version;       /* offset  4 — firmware version number */
    uint32_t size;          /* offset  8 — byte count of firmware body (after header) */
    uint32_t crc;           /* offset 12 — CRC32 of firmware body */
    uint8_t  signature[64]; /* offset 16 — P-256 ECDSA signature (r||s, 32+32 bytes) */
};                          /* total: 80 bytes */
/* Followed by 432 bytes of padding to reach 512-byte alignment */
```

### 6.2 Field descriptions

| Field       | Size | Offset | Description |
|-------------|------|--------|-------------|
| `magic`     | 4 B  | 0      | Identifies a valid app header. Bootloader rejects images where this doesn't match a compile-time constant. |
| `version`   | 4 B  | 4      | Monotonically increasing firmware version. Used for anti-rollback checks. |
| `size`      | 4 B  | 8      | Byte length of the firmware body (everything after the 512-byte header block). |
| `crc`       | 4 B  | 12     | CRC-32 computed over the firmware body. Secondary integrity check on the MCU side, faster than re-verifying ECDSA on every boot. |
| `signature` | 64 B | 16     | Raw P-256 ECDSA signature: `r` (32 bytes) concatenated with `s` (32 bytes). **Not** DER-encoded unless your signing script says otherwise. |
| padding     | 432 B | 80   | Zero or 0xFF fill to reach 512-byte boundary. |

### 6.3 Flash layout of application_signed.bin

```
 address offset     content
 ---------------    -------
 0x0000             App Header block (512 bytes)
   ├─ [0..3]        magic
   ├─ [4..7]        version
   ├─ [8..11]       size  (= N)
   ├─ [12..15]      crc32 of firmware body
   ├─ [16..79]      signature[64]
   └─ [80..511]     padding (432 bytes)
 0x0200             Firmware body (N bytes)
   └─ [0..N-1]      compiled application binary
```

---

## 7. Signing and Verification Pipeline

### 7.1 Host-side signing (build step, before transfer)

```
raw firmware binary
        │
        ▼
SHA-256(firmware body)           ← 32-byte digest
        │
        ▼
ECDSA-P256-Sign(private_key, digest)   ← 64-byte signature (r||s)
        │
        ▼
write signature → header.signature[64]
write firmware_size → header.size
compute CRC32(firmware body) → header.crc
prepend 512-byte header block
        │
        ▼
application_signed.bin    ← what the Python script reads and sends
```

SHA-256 scope: **firmware body only** (bytes 512..end of file). The header itself is excluded from the hash.

Reference: [NIST FIPS 186-5 — ECDSA](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-5.pdf)

### 7.2 MCU-side verification (bootloader, after receiving all packets)

1. Read the first 512 bytes → parse `app_header_t`.
2. Check `header.magic` against compile-time constant → reject if mismatch.
3. Check `header.version > current_version` → reject if rollback detected.
4. Compute `CRC32(firmware_body)` → compare with `header.crc` → fast integrity check.
5. Compute `SHA-256(firmware_body)` → 32-byte digest.
6. `ECDSA_P256_Verify(embedded_public_key, digest, header.signature)` → PASS or FAIL.
7. If all checks pass → mark partition valid → swap/jump to new firmware.

---

## 8. UART Configuration

```python
serial.Serial(
    port      = "/dev/ttyUSB0",
    baudrate  = 115200,
    parity    = serial.PARITY_NONE,   # no parity bit
    stopbits  = serial.STOPBITS_ONE,  # 1 stop bit
    bytesize  = serial.EIGHTBITS,     # 8 data bits → standard 8N1
    timeout   = 4,                    # 4-second read timeout
)
```

**Frame format (UART level):** 8N1 — 8 data bits, No parity, 1 stop bit.  
**Baud rate:** 115200 bps → ~11520 bytes/sec effective throughput.  
**Throughput estimate:** 263 bytes/packet ÷ 11520 bytes/sec ≈ **22.8 ms per packet** wire time, plus MCU flash write latency and handshake RTT.

---

## 9. Known Limitations

| Issue | Detail |
|-------|--------|
| No retransmission on timeout | On read timeout the transfer aborts with `break`. |
| Counter is 1-based, 32-bit | Overflow at 4 billion packets (not a real concern for any practical firmware size). |
| CRC only covers chunk | The header (counter + flag) is **not** protected by the CRC. A bit flip in the counter or flag field is undetected at the transport layer — the application header CRC32 and ECDSA cover integrity end-to-end. |
