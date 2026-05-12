#include "uart_update/update_packet_flash.h"

#include <string.h>

#include "app_header.h"
#include "custom_logger.h"
#include "flash/operations.h"

#include "custom_crc/custom_crc32.h"

bool update_packet_flash_erase_update(void) {
  //
  if (Flash_Erase_Sectors(FLASH_SECTOR_6, 2) != HAL_FLASH_ERROR_NONE) {
    custom_logger_log("Error during flash sector erase\r\n");
    return false;
  }

  return true;
}

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
