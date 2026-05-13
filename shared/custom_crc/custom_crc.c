/**
 * @file custom_crc.c
 * @brief Software implementation of CRC32 and CRC16 checksums.
 *
 * This module provides basic cyclic redundancy check (CRC) algorithms
 * used for firmware integrity validation during UART-based updates.
 *
 * Implemented algorithms:
 * - CRC32 (IEEE 802.3 polynomial 0xEDB88320)
 * - CRC16 (Modbus polynomial 0xA001)
 *
 * These checks are used to detect:
 * - Transmission corruption
 * - Flash write errors
 * - Partial or truncated firmware updates
 */

#include "custom_crc.h"

/**
 * @brief Compute CRC32 checksum over a data buffer.
 *
 * Implements a bitwise CRC32 calculation using the polynomial:
 * 0xEDB88320 (reversed IEEE 802.3 standard polynomial).
 *
 * Algorithm:
 * - Initialize CRC with 0xFFFFFFFF
 * - Process each byte bit-by-bit
 * - Apply polynomial when LSB is set
 * - Final XOR with 0xFFFFFFFF
 *
 * @param data Pointer to input data buffer.
 * @param length Length of input data in bytes.
 *
 * @return Computed CRC32 checksum.
 *
 * @note This is a software-based implementation and not optimized
 *       for performance. Hardware CRC unit is not used.
 */
uint32_t crc32(const uint8_t *data, uint32_t length) {
  uint32_t crc = 0xFFFFFFFF;

  for (uint32_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320;
      else
        crc >>= 1;
    }
  }

  return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Compute CRC16 checksum over a data buffer.
 *
 * Implements CRC16-IBM / Modbus variant using polynomial:
 * 0xA001 (reversed form of 0x8005).
 *
 * Algorithm:
 * - Initialize CRC to 0xFFFF
 * - XOR each byte into CRC
 * - Process each bit using shift and polynomial XOR
 *
 * @param data Pointer to input data buffer.
 * @param length Length of input data in bytes.
 *
 * @return Computed CRC16 checksum.
 *
 * @note Commonly used for lightweight packet validation in UART
 *       communication before full CRC32 verification.
 */
uint16_t crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF; // Initial value
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i];      // XOR byte into least sig. byte of crc
    for (int j = 8; j != 0; j--) { // Loop over each bit
      if ((crc & 0x0001) != 0) {   // If the LSB is set
        crc >>= 1;                 // Shift right and XOR polynomial
        crc ^= 0xA001;
      } else { // Else just shift right
        crc >>= 1;
      }
    }
  }
  return crc;
}
