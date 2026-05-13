#include "uart_update/update_packet_flash.h"

#include <string.h>

#include "app_header.h"
#include "custom_logger.h"
#include "flash/operations.h"

#include "custom_crc/custom_crc32.h"

#define UPDATE_PACKET_FLASH_ERASE_1_SECTOR 1
#define UPDATE_PACKET_FLASH_ERASE_2_SECTOR 2

/* 128KB */
#define UPDATE_PACKET_FLASH_SINGLE_SECTOR_SIZE (128 * 1024)

/**
 * @brief Erase update sectors
 *
 * Erase update sector storage based on firmware size. Sector 6 and 7 are both
 * 128KB, if firmware size is bigger than 128KB it will delete 2 sectors,
 * otherwise 1
 *
 * @param fw_size Size of new firmware
 * @return true/false to signal if we erased sector(s) successfully
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
 * @brief Write chunk to FLASH
 *
 * Function to write chunk into the specified destination address/update sector
 *
 * @param dest_addr Address for where to write the received chunk
 * @param payload Pointer to payload
 * @param payload_size size of payload
 * @return true/false to signal if we wrote chunk successfully or not
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
 * @brief Validate image crc32 on end of firmware TX
 *
 * Even more firmware verification. Verifies whole crc32 of the received image
 * once its written into update sector to show if the rx was done properly and
 * to check image integrity
 *
 * @param start_addr Starting address of the new firmware (!IMPORTANT: where the
 * header starts)
 * @param fw_size Firmware size grabbed from the received header
 * @return true/false if actual and expected crc32 are matching
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
