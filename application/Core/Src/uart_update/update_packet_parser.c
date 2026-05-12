#include "uart_update/update_packet_parser.h"

#include "app_header.h"
#include "custom_crc/custom_crc32.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include <string.h>

static const uint8_t _ACK = UPDATE_PACKET_ACK;
static const uint8_t _NACK = UPDATE_PACKET_NACK;

void update_packet_parser_init(struct update_packet_parser_t *parser,
                               UART_HandleTypeDef *huart,
                               TIM_HandleTypeDef *tim,
                               void (*tx_cb)(const uint8_t *)) {
  parser->huart = huart;
  parser->tim = tim;
  parser->rx_done = false;
  parser->idx = 0;
  parser->write_idx = 0;
  parser->erase_flag = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
  parser->tx_cb = tx_cb;

  // TODO: Move into some callback or something
  HAL_UART_Receive_IT(parser->huart, &parser->rx_byte, 1);
}

static void reset_buffer(struct update_packet_parser_t *parser) {
  parser->idx = 0;
  /* i was printing what was in the buffer, thats why i had null terminator
   * here, now im not doing that */
  // parser->buffer[0] = '\0';
}

static bool packet_parser_check_size(struct update_packet_parser_t *parser) {
  if (parser->idx < UPDATE_PACKET_MIN_SIZE ||
      parser->idx > UPDATE_PACKET_BUFFER_SIZE) {
    custom_logger_log("Error: packet size problem, size is {%d}\r\n",
                      parser->idx);
    reset_buffer(parser);
    parser->tx_cb(&_NACK);
    return false;
  }
  return true;
}

static bool packet_parser_compare_crc(struct update_packet_parser_t *parser) {

  if (parser->idx < 2) {
    custom_logger_log(
        "Error: Malformed package format. Size of package is: {%d}\r\n",
        parser->idx);

    reset_buffer(parser);
    // parser->tx_cb(&_NACK);
    return false;
  }

  uint16_t calculated_crc16 = crc16(parser->buffer, parser->idx - 2);
  uint16_t expected_crc16 =
      parser->buffer[parser->idx - 2] | (parser->buffer[parser->idx - 1] << 8);

  if (calculated_crc16 != expected_crc16) {
    custom_logger_log("Missmatch in crc16; expected: %d,\tactual: %d\r\n",
                      expected_crc16, calculated_crc16);

    reset_buffer(parser);
    // parser->tx_cb(&_NACK);
    return false;
  }
  return true;
}

static bool packet_parser_write_chunk(struct update_packet_parser_t *parser) {

  uint16_t words_to_write = (parser->idx - 2) / 4;

  uint32_t res = Flash_Write_Data(UPDATE_STORAGE_START_ADDR + parser->write_idx,
                                  (uint32_t *)parser->buffer, words_to_write);

  if (res != HAL_FLASH_ERROR_NONE) {
    custom_logger_log("Problem during flash write\r\n");
    reset_buffer(parser);
    return false;
  }
  parser->write_idx += parser->idx - UPDATE_PACKET_HEADER_SIZE;

  return true;
}

static bool validate_app_crc32(struct update_packet_parser_t *parser) {
  custom_logger_log(
      "Validating crc32 of the whole image that is received..\r\n");

  struct app_header_t *hdr = (struct app_header_t *)UPDATE_STORAGE_START_ADDR;
  uint32_t img_crc32 = crc32((const uint8_t *)hdr + APP_HEADER_SIZE,
                             parser->fw_size - APP_HEADER_SIZE);

  if (img_crc32 != hdr->crc) {
    custom_logger_log("Error comparing the crc32 on the end of tx\r\n");
    custom_logger_log("Expected: {%d}, Actual: {%d}\r\n", img_crc32, hdr->crc);
    // this would retry sending the package which i do not want i guess
    // parser->tx_cb(&_NACK);
    return false;
  } else {
    custom_logger_log("CRC32 matches what was expected\r\n");
  }

  return true;
}

static void packet_parser_check_rx_end(struct update_packet_parser_t *parser) {
  if (parser->write_idx == parser->fw_size) {
    /* i received everything so i can rearm erase flag to delete sector.
     * technically unneeded but ok */
    parser->erase_flag = true;

    /* TODO: Move this someplace else */
    if (!validate_app_crc32(parser)) {
      /* im already doing this 2 lines before this fn call, i dont need it here
       * honestly */
      reset_buffer(parser);
      return;
    }

    // mark tx as done, rr system
    HAL_Delay(1000);
    custom_logger_log("Restarting system...\r\n");
    HAL_Delay(1000);
    HAL_NVIC_SystemReset();
  }
}

static bool
packet_parser_check_app_header(struct update_packet_parser_t *parser) {

  struct app_header_t *hdr = (struct app_header_t *)parser->buffer;

  if (hdr->magic != APP_MAGIC_CONSTANT) {
    custom_logger_log("Error with app header in new package. Missing MAGIC "
                      "constant, couldn't verify integrity of header\r\n");
    // parser->tx_cb(&_NACK);
    return false;
  }

  /* validate if the app can fit here, but what if the user changed the size in
   * the header, i will have a problem then */
  if ((hdr->size + APP_HEADER_SIZE) > UPDATE_STORAGE_MAX_SIZE) {
    custom_logger_log("Error. Firmware cannot fit on the FLASH update sector. "
                      "MAX Size is 256KB!\r\n");
    return false;
  }

  /* valid header TODO: This function is checking app header AND assigning
   * firmware size, a bit problematic, doesnt imply it does that  */
  parser->fw_size = hdr->size + APP_HEADER_SIZE;
  parser->erase_flag = false;

  return true;
}

static bool packet_parser_erase_sector(struct update_packet_parser_t *parser) {

  if (parser->erase_flag) {

    /* only time this can error out is if theres brownout, or that code is
     * running from that sector, which doesnt happen in this case */
    if (Flash_Erase_Sectors(FLASH_SECTOR_6, 1) != HAL_FLASH_ERROR_NONE) {
      custom_logger_log("Error during flash sector erase\r\n");

      /* sending nack? retries sending package, which still means its the
       * first package and it will try again to erase. can i corrupt flash if
       * its stuck into this loop? shouldnt be like that because it failed to
       * erase */
      // parser->tx_cb(&_NACK);
      return false;
    }

    /* TODO: Separate into functions */

    if (!packet_parser_check_app_header(parser)) {
      custom_logger_log("Error during header check of the update app\r\n");
      return false;
    }
  }
  return true;
}

bool update_packet_parser_parse(struct update_packet_parser_t *parser) {

  if (!parser->rx_done) {
    return false;
  }

  parser->rx_done = false;

  /* check if package chunk size is valid */
  if (!packet_parser_check_size(parser)) {
    custom_logger_log("Error: packet size problem, size is {%d}\r\n",
                      parser->idx);
    return false;
  }

  /* compare crc16 of each received chunk */
  if (!packet_parser_compare_crc(parser)) {
    parser->tx_cb(&_NACK);
    return false;
  }

  /* this is executed on the first package, and resetted on the last package
   */
  if (!packet_parser_erase_sector(parser)) {
    custom_logger_log("Error, failed erasing sector, retrying..\r\n");
    parser->tx_cb(&_NACK);
    return false;
  }

  if (!packet_parser_write_chunk(parser)) {
    custom_logger_log("Error, failed writing chunk\r\n");
    parser->tx_cb(&_NACK);

    return false;
  }

  reset_buffer(parser);
  /* everything is ok so i can ACK this package and get the next chunk of
   * data*/
  parser->tx_cb(&_ACK);

  /* TODO: Testing, moved this here so that i can confirm/ACK the last package
   * i received and only after that do i check if we are at end of tx, since
   * this will reboot the system */
  packet_parser_check_rx_end(parser);

  return true;
}

void update_packet_parser_uart_callback(struct update_packet_parser_t *parser,
                                        UART_HandleTypeDef *huart) {
  if (huart->Instance == parser->huart->Instance) {
    HAL_TIM_Base_Stop_IT(parser->tim);
    // __HAL_TIM_SET_COUNTER(parser->tim, 0);
    // CLEAR_BIT(parser->tim->Instance->CR1, TIM_CR1_OPM);

    parser->buffer[parser->idx++] = parser->rx_byte;

    HAL_UART_Receive_IT(parser->huart, &parser->rx_byte, 1);
    HAL_TIM_Base_Start_IT(parser->tim);
  }
}

void update_packet_parser_tim_callback(struct update_packet_parser_t *parser,
                                       TIM_HandleTypeDef *htim) {

  if (htim->Instance == parser->tim->Instance) {
    // os timer sent interrupt
    // i would need to restart it

    HAL_TIM_Base_Stop_IT(parser->tim);
    custom_logger_log("Timer fired, idx=%d\r\n", parser->idx);
    // CRC is 2 bytes, so i need at least 3 bytes package
    if (parser->idx > 2) {
      // i received whole chunk i need to tell timer to print it by disabling it
      // HAL_UART_Transmit_IT(&huart1, &_ACK, 1);
      parser->rx_done = true;
    } else {
      // custom_logger_log("Nack\r\n");
      // HAL_UART_Transmit_IT(&huart1, &_NACK, 1);
      parser->tx_cb(&_NACK);
      parser->idx = 0;
    }
  }
}
