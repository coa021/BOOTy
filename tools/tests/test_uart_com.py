#!/usr/bin/env python3
"""
UART OTA Firmware Update Test Suite, STM32
============================================

Tests edge cases in the OTA update protocol (chunk-level CRC16 + ACK/NACK handshake).

ADJUST THESE CONSTANTS TO MATCH YOUR ACTUAL HEADER STRUCT:
  MAGIC_OFFSET, VERSION_OFFSET, FW_SIZE_OFFSET, CRC32_OFFSET,
  SIGNATURE_OFFSET, SIGNATURE_LEN, VALID_MAGIC
"""

import serial
import struct
import time
import zlib
import crcmod
import logging
import argparse
from dataclasses import dataclass, field
from typing import Optional
from enum import IntEnum

#  Protocol constants

ACK = 0x01
NACK = 0x15
CHUNK_SIZE = 256  # bytes per data chunk (not counting the 2-byte CRC16 trailer)
HEADER_SIZE = 512  # app header size in bytes
UART_TIMEOUT = 2.0  # seconds to wait for ACK/NACK

#  App header field offsets
# ADJUST THESE to match your actual bootloader header struct.
# These are placeholder guesses, verify against your bl_app_header_t definition.

MAGIC_OFFSET = 0  # uint32_t magic
VERSION_OFFSET = 4  # uint32_t version
FW_SIZE_OFFSET = 8  # uint32_t fw_size (size of the application, excluding header)
CRC32_OFFSET = 12  # uint32_t crc32 (CRC32 of the application data)
SIGNATURE_OFFSET = 16  # uint8_t signature[N]
SIGNATURE_LEN = 64  # adjust: 64 for ECDSA P-256 raw (r||s), 256 for RSA-2048

VALID_MAGIC = 0x0B00B1E5  # replace with your actual magic constant

FLASH_APP_SECTOR_SIZE = 256 * 1024  # 256 KB, adjust if your partition is different


#  CRC helpers (identical to your sender script)


def compute_crc16(data: bytes) -> bytes:
    """CRC-16/ARC, poly=0x8005, init=0xFFFF, refin=True, refout=True, xorout=0x0000"""
    fn = crcmod.mkCrcFun(0x18005, rev=True, initCrc=0xFFFF, xorOut=0x0000)
    return struct.pack("<H", fn(data))


def compute_crc32(data: bytes) -> bytes:
    return struct.pack("<I", zlib.crc32(data) & 0xFFFFFFFF)


#  Result types
class Response(IntEnum):
    ACK = 1
    NACK = 2
    TIMEOUT = 3
    ERROR = 4  # unexpected byte


@dataclass
class ChunkResult:
    chunk_idx: int
    byte_range: tuple[int, int]
    response: Response
    note: str = ""


@dataclass
class TestResult:
    name: str
    passed: bool
    expected: str  # human-readable description of what SHOULD happen
    summary: str  # what actually happened
    chunks: list[ChunkResult] = field(default_factory=list)

    def __str__(self) -> str:
        status = "PASS" if self.passed else "FAIL"
        lines = [f"[{status}] {self.name}"]
        lines.append(f"  Expected : {self.expected}")
        lines.append(f"  Got      : {self.summary}")
        if self.chunks:
            for c in self.chunks:
                lines.append(
                    f"    chunk {c.chunk_idx:>4} [{c.byte_range[0]:>7}..{c.byte_range[1]:>7}]: {c.response.name}  {c.note}"
                )
        return "\n".join(lines)


#  UART link
class UARTLink:
    def __init__(
        self, port: str, baudrate: int = 115200, timeout: float = UART_TIMEOUT
    ):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=timeout,
        )
        self.log = logging.getLogger("uart")

    def send_raw(self, payload: bytes) -> Response:
        """Write bytes, block until ACK/NACK/timeout. No retry."""
        self.ser.write(payload)
        resp = self.ser.read(1)
        if not resp:
            return Response.TIMEOUT
        b = resp[0]
        if b == ACK:
            return Response.ACK
        if b == NACK:
            return Response.NACK
        self.log.warning(f"Unexpected byte: 0x{b:02X}")
        return Response.ERROR

    def flush(self):
        """Drain both buffers between tests to avoid state bleed."""
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

    def close(self):
        self.ser.close()


#  Chunk sender (no retry, for test use)
def send_chunk(link: UARTLink, data: bytes, corrupt_crc: bool = False) -> Response:
    """
    Append CRC16 and send one chunk. If corrupt_crc=True, flips the high
    byte of the CRC so the MCU's CRC check must fail.
    """
    crc = compute_crc16(data)
    if corrupt_crc:
        # Guarantee a mismatch, flip the high byte
        crc = bytes([crc[0], crc[1] ^ 0xFF])
    return link.send_raw(data + crc)


def send_firmware_chunks(
    link: UARTLink,
    firmware: bytes,
    chunk_size: int = CHUNK_SIZE,
    corrupt_chunk_idx: Optional[int] = None,
    stop_on_nack: bool = True,
) -> list[ChunkResult]:
    """
    Send firmware in chunks, logging each ACK/NACK/TIMEOUT.
    Does NOT retry on NACK, that's intentional for test observability.

    Args:
        corrupt_chunk_idx: if set, that chunk's CRC16 will be deliberately wrong.
        stop_on_nack: stop transmission on first non-ACK (default True).
    """
    results: list[ChunkResult] = []
    sent = 0
    idx = 0

    while sent < len(firmware):
        chunk = bytes(firmware[sent : sent + chunk_size])
        corrupt = corrupt_chunk_idx is not None and idx == corrupt_chunk_idx
        response = send_chunk(link, chunk, corrupt_crc=corrupt)
        byte_end = sent + len(chunk)

        cr = ChunkResult(
            chunk_idx=idx,
            byte_range=(sent, byte_end),
            response=response,
            note="CRC deliberately corrupted" if corrupt else "",
        )
        results.append(cr)

        if response != Response.ACK and stop_on_nack:
            break

        sent += len(chunk)
        idx += 1

    return results


#  Test cases


def test_oversized_chunk(link: UARTLink) -> TestResult:
    """
    Send 300-byte chunk (CHUNK_SIZE + 44 bytes extra).
    Your MCU's packet_parser_check_size() should catch this and NACK
    (or timeout if it waits for more bytes before deciding).

    Ref: your packet_parser_check_size() logic, if it checks
    that idx == CHUNK_SIZE + 2 (data + CRC), it will reject 302.
    """
    oversized = bytes(range(256)) + bytes(range(44))  # 300 bytes
    crc = compute_crc16(oversized)
    response = link.send_raw(oversized + crc)  # 302 bytes total

    passed = response in (Response.NACK, Response.TIMEOUT)
    return TestResult(
        name="oversized_chunk",
        passed=passed,
        expected="NACK or TIMEOUT, MCU size check rejects >256-byte chunk",
        summary=f"Got {response.name}",
    )


def test_undersized_chunk(link: UARTLink) -> TestResult:
    """
    Send 1-byte chunk. Total payload is 3 bytes (1 data + 2 CRC).
    MCU size check should reject anything smaller than some minimum.

    NOTE: If your size check only rejects things larger than CHUNK_SIZE and has
    no lower bound check, this test will reveal that gap, the MCU might ACK it.
    """
    tiny = bytes([0xAB])
    crc = compute_crc16(tiny)
    response = link.send_raw(tiny + crc)

    passed = response in (Response.NACK, Response.TIMEOUT)
    return TestResult(
        name="undersized_chunk",
        passed=passed,
        expected="NACK or TIMEOUT, MCU size check rejects 1-byte chunk",
        summary=f"Got {response.name}",
    )


def test_corrupted_crc16(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Take the first 256-byte chunk from real firmware, deliberately mangle its CRC16.
    MCU's packet_parser_compare_crc() must NACK this.
    """
    chunk = bytes(firmware[:CHUNK_SIZE])
    response = send_chunk(link, chunk, corrupt_crc=True)

    passed = response == Response.NACK
    return TestResult(
        name="corrupted_crc16",
        passed=passed,
        expected="NACK, CRC16 mismatch on first chunk",
        summary=f"Got {response.name}",
    )


def test_firmware_without_header(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Strip the 512-byte header and send raw application binary.
    The first chunk the MCU receives won't have a valid magic constant
    so it should fail app header parsing.

    CAVEAT: your parser_check_rx_end() or a separate header-validation
    step must explicitly check magic on the first chunk; if it doesn't,
    the MCU may happily flash garbage. This test tells you if that
    validation exists.
    """
    app_only = firmware[HEADER_SIZE:]
    chunks = send_firmware_chunks(link, app_only)

    first_fail = next((c for c in chunks if c.response != Response.ACK), None)
    passed = first_fail is not None

    summary = (
        f"Failed on chunk {first_fail.chunk_idx} with {first_fail.response.name}"
        if first_fail
        else f"All {len(chunks)} chunks ACKed, MCU did NOT reject missing header (bug!)"
    )
    return TestResult(
        name="firmware_without_header",
        passed=passed,
        expected="NACK on first chunk, MCU rejects missing/wrong header",
        summary=summary,
        chunks=chunks,
    )


def test_invalid_magic(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Overwrite the magic field in the app header with 0xDEADDEAD.
    MCU must reject this at header parsing.
    """
    fw = bytearray(firmware)
    struct.pack_into("<I", fw, MAGIC_OFFSET, 0xDEADDEAD)

    chunks = send_firmware_chunks(link, bytes(fw))
    first_fail = next((c for c in chunks if c.response != Response.ACK), None)
    passed = first_fail is not None

    summary = (
        f"Failed on chunk {first_fail.chunk_idx} with {first_fail.response.name}"
        if first_fail
        else f"All {len(chunks)} chunks ACKed, magic check not enforced (bug!)"
    )
    return TestResult(
        name="invalid_magic",
        passed=passed,
        expected="NACK, MCU rejects bad magic constant in app header",
        summary=summary,
        chunks=chunks,
    )


def test_invalid_fw_size_too_large(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Set fw_size in the header to FLASH_APP_SECTOR_SIZE + 1 (just over the limit).
    MCU should detect this and NACK before or during the first flash write.

    Note: we send actual real-sized firmware, we're only lying about the size
    field in the header.
    """
    fw = bytearray(firmware)
    struct.pack_into("<I", fw, FW_SIZE_OFFSET, FLASH_APP_SECTOR_SIZE + 1)

    chunks = send_firmware_chunks(link, bytes(fw))
    first_fail = next((c for c in chunks if c.response != Response.ACK), None)
    passed = first_fail is not None

    summary = (
        f"Failed on chunk {first_fail.chunk_idx} with {first_fail.response.name}"
        if first_fail
        else f"All {len(chunks)} chunks ACKed, fw_size bounds check not enforced (bug!)"
    )
    return TestResult(
        name="invalid_fw_size_too_large",
        passed=passed,
        expected=f"NACK, fw_size > {FLASH_APP_SECTOR_SIZE // 1024}KB should be rejected",
        summary=summary,
        chunks=chunks,
    )


def test_corrupt_header_crc32(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Flip the CRC32 field in the app header. Leave all chunk data intact.

    IMPORTANT: Per-chunk CRC16 will still pass for every chunk because
    the raw bytes are transmitted correctly, the CRC16 covers what was sent,
    not what it means. The MCU's full-firmware CRC32 validation happens
    *after* the last chunk is received (in your packet_parser_check_rx_end or
    similar). So you expect all chunks to ACK, but the MCU should not
    reboot / swap partitions. Verify via MCU logs.
    """
    fw = bytearray(firmware)
    existing = struct.unpack_from("<I", fw, CRC32_OFFSET)[0]
    struct.pack_into("<I", fw, CRC32_OFFSET, existing ^ 0xFFFFFFFF)

    chunks = send_firmware_chunks(link, bytes(fw), stop_on_nack=False)
    n_acked = sum(1 for c in chunks if c.response == Response.ACK)

    # "passed" here means: all chunks ACKed (per-chunk protocol OK),
    # which is the expected behavior. The CRC32 rejection is MCU-side post-transfer.
    passed = n_acked == len(chunks)

    return TestResult(
        name="corrupt_header_crc32",
        passed=passed,
        expected=(
            "All chunks ACK (per-chunk CRC16 is fine). "
            "MCU must reject full firmware on post-transfer CRC32 check. "
            "Verify: MCU should NOT reboot into new app."
        ),
        summary=f"{n_acked}/{len(chunks)} chunks ACKed",
        chunks=chunks,
    )


def test_corrupt_single_app_byte(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Flip one byte inside the application body (past the header).
    The chunk containing that byte will have a CRC16 mismatch → NACK.
    Chunks before it should ACK normally.

    We target a byte in chunk index 1 of the application body
    (i.e., offset HEADER_SIZE + CHUNK_SIZE + 10).
    """
    fw = bytearray(firmware)
    target_offset = HEADER_SIZE + CHUNK_SIZE + 10
    fw[target_offset] ^= 0xFF

    chunks = send_firmware_chunks(link, bytes(fw))
    first_fail = next((c for c in chunks if c.response != Response.ACK), None)

    expected_fail_chunk = (HEADER_SIZE + CHUNK_SIZE + 10) // CHUNK_SIZE
    passed = first_fail is not None and first_fail.chunk_idx == expected_fail_chunk

    summary = (
        f"Failed on chunk {first_fail.chunk_idx} with {first_fail.response.name}"
        if first_fail
        else f"All {len(chunks)} chunks ACKed, corrupted byte not caught (bug!)"
    )
    return TestResult(
        name="corrupt_single_app_byte",
        passed=passed,
        expected=f"NACK on chunk {expected_fail_chunk} (byte at offset {target_offset} flipped)",
        summary=summary,
        chunks=chunks,
    )


def test_invalid_signature(link: UARTLink, firmware: bytes) -> TestResult:
    """
    Zero out the signature field in the app header.

    Same caveat as corrupt_header_crc32: per-chunk CRC16 still passes.
    Signature verification is post-transfer on the MCU side.
    Verify by checking MCU logs / no partition swap occurs.
    """
    fw = bytearray(firmware)
    for i in range(SIGNATURE_LEN):
        fw[SIGNATURE_OFFSET + i] = 0x00

    chunks = send_firmware_chunks(link, bytes(fw), stop_on_nack=False)
    n_acked = sum(1 for c in chunks if c.response == Response.ACK)
    passed = n_acked == len(chunks)

    return TestResult(
        name="invalid_signature",
        passed=passed,
        expected=(
            "All chunks ACK (per-chunk CRC16 fine). "
            "MCU must reject on signature verification post-transfer. "
            "Verify: MCU should NOT reboot into new app."
        ),
        summary=f"{n_acked}/{len(chunks)} chunks ACKed",
        chunks=chunks,
    )


#  Flash write error note


def note_flash_write_errors():
    """
    Flash write errors (HAL_FLASH_Program returning HAL_ERROR) cannot be
    triggered from the host side without MCU-side cooperation.

    Your options:
      1. Bootloader debug hook: add a secret command (e.g., CMD_TEST_FLASH_FAIL)
         that sets a flag causing the next write to return failure.
      2. Write-protect the target sector before the test using
         HAL_FLASHEx_OBProgram() with WRP bits, then the write will fault.
         Reference: RM0383 §3.6, HAL_FLASHEx_OBProgram in stm32f4xx_hal_flash_ex.h
      3. Mock HAL_FLASH_Program in a unit test environment on-device.

    This test suite does not attempt to trigger flash write errors remotely.
    """
    logging.getLogger("uart_test").info(
        "\n[NOTE] Flash write error tests require MCU-side instrumentation. "
        "See note_flash_write_errors() docstring for options."
    )


#  Test runner


def run_all_tests(port: str, firmware_path: str, baudrate: int = 115200):
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s  %(message)s",
        datefmt="%H:%M:%S",
    )
    log = logging.getLogger("uart_test")

    with open(firmware_path, "rb") as f:
        firmware = f.read()

    log.info(f"Firmware: {firmware_path} ({len(firmware)} bytes)")
    log.info(f"Port    : {port} @ {baudrate} baud\n")

    link = UARTLink(port=port, baudrate=baudrate)

    # (name, factory) pairs, each factory returns a TestResult
    tests = [
        ("Oversized chunk (>256B)", lambda: test_oversized_chunk(link)),
        ("Undersized chunk (<2B)", lambda: test_undersized_chunk(link)),
        ("Corrupted CRC16", lambda: test_corrupted_crc16(link, firmware)),
        (
            "Firmware without header",
            lambda: test_firmware_without_header(link, firmware),
        ),
        ("Invalid magic constant", lambda: test_invalid_magic(link, firmware)),
        (
            "Invalid fw_size (>flash limit)",
            lambda: test_invalid_fw_size_too_large(link, firmware),
        ),
        # ("Corrupt header CRC32", lambda: test_corrupt_header_crc32(link, firmware)),
        (
            "Corrupt single app byte",
            lambda: test_corrupt_single_app_byte(link, firmware),
        ),
        ("Invalid signature", lambda: test_invalid_signature(link, firmware)),
    ]

    results: list[TestResult] = []

    for name, factory in tests:
        log.info(f"{''*60}")
        log.info(f"TEST: {name}")
        link.flush()
        time.sleep(1)  # let MCU settle between tests

        try:
            result = factory()
            results.append(result)
            log.info(str(result))
        except Exception as exc:
            log.error(f"Exception in '{name}': {exc}", exc_info=True)
            results.append(
                TestResult(
                    name=name,
                    passed=False,
                    expected="no exception",
                    summary=f"Exception: {exc}",
                )
            )

        time.sleep(0.2)

    #  Summary
    log.info(f"\n{'═'*60}")
    log.info("SUMMARY")
    log.info(f"{'═'*60}")

    passed_count = sum(1 for r in results if r.passed)
    log.info(f"Passed: {passed_count} / {len(results)}\n")

    for r in results:
        tag = "✓" if r.passed else "✗"
        log.info(f"  {tag}  {r.name}")

    note_flash_write_errors()
    link.close()


#  Entry point

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="STM32 UART OTA Test Suite")
    parser.add_argument("--port", default="/dev/ttyUSB2", help="Serial port")
    parser.add_argument(
        "--firmware", default="../application_signed.bin", help="Signed firmware binary"
    )
    parser.add_argument("--baud", default=115200, type=int, help="Baud rate")
    args = parser.parse_args()

    run_all_tests(args.port, args.firmware, args.baud)
