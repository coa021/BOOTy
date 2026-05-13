/**
 * @file update_packet_flash.c
 * @brief Flash memory operations for UART firmware update packets.
 *
 * This module provides helper functions for:
 * - Erasing update storage sectors
 * - Writing firmware chunks into flash memory
 * - Validating final firmware image CRC32 integrity
 *
 * Firmware updates are stored inside dedicated update flash sectors
 * before being verified and applied by the bootloader.
 */

#include "uart_update/update_packet_flash.h"

#include <string.h>

#include "app_header.h"
#include "custom_logger.h"
#include "flash/operations.h"

#include "custom_crc/custom_crc.h"

/**
 * @brief Number of sectors to erase for small firmware images.
 */
#define UPDATE_PACKET_FLASH_ERASE_1_SECTOR 1

/**
 * @brief Number of sectors to erase for large firmware images.
 */
#define UPDATE_PACKET_FLASH_ERASE_2_SECTOR 2

/**
 * @brief Size of a single update storage flash sector in bytes.
 *
 * Sectors 6 and 7 are both 128 KB.
 */
#define UPDATE_PACKET_FLASH_SINGLE_SECTOR_SIZE (128 * 1024)

/**
 * @brief Erase firmware update storage sectors.
 *
 * Determines how many sectors must be erased depending on the incoming
 * firmware size.
 *
 * Erase strategy:
 * - Firmware <= 128 KB:
 *   - Erase 1 sector
 * - Firmware > 128 KB:
 *   - Erase 2 sectors
 *
 * @param fw_size Size of the incoming firmware image in bytes.
 *
 * @retval true  Flash sectors erased successfully.
 * @retval false Flash erase operation failed.
 *
 */
bool update_packet_flash_erase_update(const uint32_t fw_size) {
  //
  uint32_t num_sectors = fw_size > UPDATE_PACKET_FLASH_SINGLE_SECTOR_SIZE
                             ? UPDATE_PACKET_FLASH_ERASE_2_SECTOR
                             : UPDATE_PACKET_FLASH_ERASE_1_SECTOR;
  custom_logger_log("Deleting %d sector(s)\r\n", num_sectors);

  if (Flash_Erase_Sectors(FLASH_SECTOR_6, num_sectors) !=
      HAL_FLASH_ERROR_NONE) {
    custom_logger_log("Error during flash sector erase\r\n");
    return false;
  }

  return true;
}

/**
 * @brief Write a received firmware chunk into flash memory.
 *
 * Writes a firmware packet payload into the update storage flash region.
 *
 * The function:
 * - Writes aligned 32-bit words directly
 * - Handles remaining unaligned bytes separately
 * - Pads partial words with `0xFF`
 *
 * @param dest_addr Destination flash address.
 * @param payload Pointer to received firmware payload.
 * @param payload_size Payload size in bytes.
 *
 * @retval true  Flash write completed successfully.
 * @retval false Flash write operation failed.
 *
 */
bool update_packet_flash_write_chunk(uint32_t dest_addr, const uint8_t *payload,
                                     uint16_t payload_size) {
  //

  uint16_t words_to_write = payload_size / 4;
  uint16_t word_remainder = payload_size % 4;

  if (words_to_write > 0) {
    if (Flash_Write_Data(dest_addr, (const uint32_t *)payload,
                         words_to_write) != HAL_FLASH_ERROR_NONE) {
      custom_logger_log("Problem during flash write\r\n");

      return false;
    }
  }
  /* writing remainder that is not a word */
  if (word_remainder > 0) {
    uint32_t last_word = 0xFFFFFFFF;
    memcpy(&last_word, payload + (words_to_write * 4), word_remainder);

    if (Flash_Write_Data(dest_addr + (words_to_write * 4), &last_word, 1) !=
        HAL_FLASH_ERROR_NONE) {
      custom_logger_log("Error: flash write remainder failed\r\n");

      return false;
    }
  }

  return true;
}

/**
 * @brief Validate CRC32 integrity of the fully received firmware image.
 *
 * Performs a final CRC32 verification after the entire firmware image
 * has been written into the update flash storage region.
 *
 * The CRC is calculated over the firmware payload excluding the
 * application header.
 *
 * The computed CRC32 is compared against the CRC value stored
 * inside the firmware header.
 *
 * @param start_addr Starting address of the firmware image.
 *                   This address must point to the application header.
 * @param fw_size Total firmware image size in bytes including header.
 *
 * @retval true  Firmware image CRC32 matches expected value.
 * @retval false CRC32 mismatch detected.
 *
 * @warning This function validates integrity only.
 *          Cryptographic authenticity is verified separately using
 *          signature verification.
 */
bool update_packet_flash_validate_image_crc32(uint32_t start_addr,
                                              uint32_t fw_size) {
  //
  custom_logger_log(
      "Validating crc32 of the whole image that is received..\r\n");

  struct app_header_t *hdr = (struct app_header_t *)start_addr;
  uint32_t img_crc32 =
      crc32((const uint8_t *)hdr + APP_HEADER_SIZE, fw_size - APP_HEADER_SIZE);

  if (img_crc32 != hdr->crc) {
    custom_logger_log("Error comparing the crc32 on the end of tx\r\n");
    custom_logger_log("Expected: {%d}, Actual: {%d}\r\n", img_crc32, hdr->crc);

    return false;
  } else {
    custom_logger_log("CRC32 matches what was expected\r\n");
  }

  return true;
}
