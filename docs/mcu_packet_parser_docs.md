# MCU-Side OTA Packet Processing

**Files:** `update_packet_parser.c` · `update_packet_validator.c` · `update_packet_flash.c`  
**MCU:** STM32F411 Black Pill · HAL · UART interrupt + TIM one-pulse framing

---

## 1. Architecture Overview

The design separates three concerns:

| Module | Responsibility |
|--------|---------------|
| `update_packet_parser` | Owns the `update_packet_parser_t` state machine. Feeds the ISR callbacks, runs the main-loop parse step. |
| `update_packet_validator` | Pure validation functions — size, CRC16, app header magic/size. No side effects on parser state. |
| `update_packet_flash` | Flash erase, chunk write, and post-receive CRC32 image validation. |

The parser is the only caller of both validator and flash modules. Validators never touch flash; flash never validates protocol.

---

## 2. Reception Mechanism — UART ISR + TIM Framing

Reception uses a **byte-at-a-time UART interrupt** combined with a **hardware timer** to detect end-of-packet.

### 2.1 UART ISR (`update_packet_parser_uart_callback`)

Called by `HAL_UART_RxCpltCallback` on every received byte:

```
1. HAL_TIM_Base_Stop_IT(tim)       ← stop the timeout timer
2. buffer[idx++] = rx_byte         ← append byte to buffer
3. HAL_UART_Receive_IT(huart, &rx_byte, 1)  ← re-arm for next byte
4. HAL_TIM_Base_Start_IT(tim)      ← restart timeout from 0
```

The timer **never expires while bytes keep arriving**. It only fires if no byte is received within its configured period (50ms in this implementation).

This is a standard inter-character timeout pattern for framing variable-length packets without a length field in the header.  
Reference: ST Application Note AN4655 — *Implementing a Simple UART Protocol with HAL*

### 2.2 TIM ISR (`update_packet_parser_tim_callback`)

Fires 50ms after the last byte:

```c
HAL_TIM_Base_Stop_IT(tim);
if (parser->idx > UPDATE_PACKET_OVERHEAD_SIZE) {
    parser->rx_done = true;          // signal main loop
} else {
    parser->tx_cb(&_NACK);           // too short, reject immediately
    parser->idx = 0;
}
```

**Threshold:** `UPDATE_PACKET_OVERHEAD_SIZE` = 7 bytes (4B counter + 1B flag + 2B CRC16). A packet shorter than this cannot contain any payload.

### 2.3 Main loop

```c
while (1) {
    update_packet_parser_parse_and_process(&parser);
}
```

`parse_and_process` polls `rx_done`. If false it returns immediately. ISRs are never blocked waiting for the main loop.

---

## 3. Buffer Layout

When `rx_done` becomes true, `parser->buffer` contains the raw bytes exactly as received from the host:

```
 buffer index   size    field               type
 ------------   ----    -----               ----
 [0..3]         4 B     package_counter     uint32_t, LE
 [4]            1 B     tx_end flag         uint8_t  (0x00 or 0x01)
 [5..5+N-1]     N B     firmware chunk      uint8_t[]
 [5+N..5+N+1]   2 B     CRC16               uint16_t, LE
```

Where N = `payload_size` = `received - OVERHEAD_SIZE` = `received - 7`.

Constants derived from the above:

| Constant | Value | Meaning |
|----------|-------|---------|
| `UPDATE_PACKET_HEADER_SIZE` | 5 | bytes before payload (counter + flag) |
| `UPDATE_PACKET_CRC16_SIZE` | 2 | bytes of CRC appended after payload |
| `UPDATE_PACKET_OVERHEAD_SIZE` | 7 | header + CRC (no payload content) |
| `UPDATE_PACKET_BUFFER_SIZE` | 263 | max packet = 5 + 256 + 2 |
| `UPDATE_PACKET_MIN_SIZE` | > 7 | at least 1 byte of payload |
| `UPDATE_PACKET_FIRST_PACKET_INDEX` | 1 | expected counter value for first packet |

---

## 4. `parse_and_process` — Validation Chain

All steps are sequential. Any failure sends NACK and returns false. No step is skipped.

### Step 1 — Guard: `rx_done`

```c
if (!parser->rx_done) return false;
parser->rx_done = false;
```

Clears the flag atomically in the main loop. The ISR sets it; the main loop clears it.

### Step 2 — Extract counter and flag

```c
memcpy(&parser->current_counter, parser->buffer, sizeof(uint32_t));
memcpy(&parser->tx_end, parser->buffer + sizeof(uint32_t), sizeof(uint8_t));
```

`memcpy` is used instead of pointer cast to avoid strict-aliasing UB and potential unaligned access faults on Cortex-M.  
Reference: [GCC strict aliasing and `memcpy`](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-fstrict-aliasing)

### Step 3 — Counter sequence check

```c
if ((parser->previous_counter + 1) != parser->current_counter) {
    // NACK
}
```

Detects: duplicate packets, out-of-order delivery, missed packets. `previous_counter` starts at 0; first expected `current_counter` is 1 (`UPDATE_PACKET_FIRST_PACKET_INDEX`).

### Step 4 — Size validation (`update_packet_validate_size`)

```c
if (received < UPDATE_PACKET_MIN_SIZE || received > UPDATE_PACKET_BUFFER_SIZE)  → NACK
if (write_idx > max_size)  → NACK
```

Two independent checks: packet is not malformed, and we have not overflowed the update storage sector.

### Step 5 — CRC16 validation (`update_packet_validate_crc16`)

```c
uint16_t payload_size    = received - UPDATE_PACKET_OVERHEAD_SIZE;
uint16_t calculated_crc16 = crc16(buffer + UPDATE_PACKET_HEADER_SIZE, payload_size);

uint16_t crc_offset   = received - UPDATE_PACKET_CRC16_SIZE;
uint16_t expected_crc16 = (uint16_t)buffer[crc_offset] | (uint16_t)(buffer[crc_offset + 1] << 8);
```

CRC is computed over `buffer[5..received-3]` — **payload bytes only**. The header (counter + flag) is not covered by CRC16. The expected value is read from the last 2 bytes of the buffer as little-endian.

### Step 6 — First-packet branch (`current_counter == 1`)

Only runs on the first packet, which must contain the full 512-byte app header at `buffer[HEADER_SIZE]`.

#### 6a — App header validation (`update_packet_validate_app_header`)

```c
struct app_header_t *hdr = (struct app_header_t *)(buffer + UPDATE_PACKET_HEADER_SIZE);

if (hdr->magic != APP_MAGIC_CONSTANT)  → NACK
if ((hdr->size + APP_HEADER_SIZE) > APP_MAX_SIZE)  → NACK

*out_size = hdr->size + APP_HEADER_SIZE;   // total image size incl. header
```

`hdr->size` is the firmware body length (bytes 512..end). `out_size` stores the **total** including the 512-byte header block. This value is later used as the scope for the final CRC32.

**Risk note:** if an attacker can craft a header with a falsified `size` field, the CRC32 scope changes. ECDSA verification (not shown in this module — expected in bootloader jump logic) prevents this.

#### 6b — Flash erase (`update_packet_flash_erase_update`)

```c
uint32_t num_sectors = fw_size > (128 * 1024) ? 2 : 1;
Flash_Erase_Sectors(FLASH_SECTOR_6, num_sectors);
```

STM32F411 sectors 6 and 7 are each 128KB. The erase always starts at sector 6. Sector 7 is only erased if the firmware body exceeds 128KB. Erase must happen before the first write — this is why it is gated on `current_counter == 1`.

### Step 7 — Flash write (`update_packet_flash_write_chunk`)

```c
dest = UPDATE_STORAGE_START_ADDR + parser->write_idx;
src  = parser->buffer + UPDATE_PACKET_HEADER_SIZE;   // skip counter + flag
size = payload_size;                                 // not including CRC or header
```

STM32F4 flash requires **32-bit word-aligned writes**:

```c
uint16_t words_to_write = payload_size / 4;
uint16_t word_remainder  = payload_size % 4;

// full words
Flash_Write_Data(dest, (uint32_t *)payload, words_to_write);

// remainder: pack into 0xFFFFFFFF-initialized word via memcpy, write 1 word
uint32_t last_word = 0xFFFFFFFF;
memcpy(&last_word, payload + (words_to_write * 4), word_remainder);
Flash_Write_Data(dest + (words_to_write * 4), &last_word, 1);
```

`0xFFFFFFFF` initialization matters: unprogrammed flash bits on STM32 read as 1. Padding with 1s avoids toggling bits that were never meant to be written.  
Reference: [STM32F411 Reference Manual RM0383, Section 3.5 — Flash programming](https://www.st.com/resource/en/reference_manual/rm0383-stm32f411xce-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

### Step 8 — ACK and state advance

```c
parser->write_idx       += payload_size;
parser->previous_counter = parser->current_counter;
reset_buffer(parser);       // idx = 0 only, buffer not zeroed
parser->tx_cb(&_ACK);
```

`reset_buffer` only resets `idx`. The buffer bytes are not zeroed between packets — they get overwritten by the next packet's bytes.

`update_packet_parser_reset` (full reset) additionally zeroes the buffer with `memset` and is only used on fatal errors or after a failed CRC32 at end of transfer.

### Step 9 — End-of-transfer check

```c
if (parser->tx_end) {
    // validate CRC32 of whole image in flash
    // on success: HAL_NVIC_SystemReset()
    // on fail: update_packet_parser_reset()
}
```

### Step 10 — Final CRC32 (`update_packet_flash_validate_image_crc32`)

```c
struct app_header_t *hdr = (struct app_header_t *)start_addr;
uint32_t img_crc32 = crc32(
    (const uint8_t *)hdr + APP_HEADER_SIZE,   // body starts at offset 512
    fw_size - APP_HEADER_SIZE                  // body byte count
);
if (img_crc32 != hdr->crc) → reset parser, no reboot
```

The header is cast directly from the flash address — this is valid because STM32F4 flash is memory-mapped and readable via normal pointer dereference.  

CRC32 scope: **firmware body only**, consistent with how the host computed `hdr->crc` during the signing step. A mismatch here means either the UART transfer corrupted data that the per-packet CRC16 missed, or the image was tampered with after signing.

On CRC32 match: `HAL_NVIC_SystemReset()` — hard reset, bootloader will pick up the new image.

---

## 5. Error Handling Summary

| Failure point | Response | Recovery |
|---------------|----------|----------|
| rx_done false | none | main loop retries |
| Counter mismatch | NACK + `reset_buffer` | host should retransmit; currently no retry on host |
| Size out of range | NACK + `reset_buffer` | same |
| CRC16 mismatch | NACK + `reset_buffer` | same |
| Bad magic in header | NACK + `reset_buffer` | same |
| Firmware too large for flash | NACK + `reset_buffer` | same |
| Flash erase error | NACK + `reset_buffer` | same |
| Flash write error | NACK + `reset_buffer` | same |
| CRC32 image mismatch (end of TX) | `parser_reset()` (full) | MCU ready to accept a new image |
| CRC32 image match | `HAL_NVIC_SystemReset()` | system reboots into bootloader |

`reset_buffer` resets only `idx`. `parser_reset` additionally clears `rx_done`, `write_idx`, and `memset`s the buffer.

---

## 6. Known Issues / TODOs in Code

| Issue | Location | Notes |
|-------|----------|-------|
| `HAL_UART_Receive_IT` called in `init` directly | `update_packet_parser_init` | comment says "TODO: Move into callback or something" |
| `fw_size` header-size dependency | `validate_app_header` | `out_size = hdr->size + APP_HEADER_SIZE`; if host writes wrong size, CRC32 scope is wrong |
| No ECDSA verify in this module | missing | header has `signature[64]` field; not verified here — assumed done in bootloader jump logic |
