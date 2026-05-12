#include "uart_update/update_packet_parser.h"

#include "app_header.h"
#include "custom_crc/custom_crc32.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include <string.h>

#include "uart_update/update_packet_flash.h"
#include "uart_update/update_packet_validator.h"

static const uint8_t _ACK = UPDATE_PACKET_ACK;
static const uint8_t _NACK = UPDATE_PACKET_NACK;

static void update_packet_parser_reset(struct update_packet_parser_t *parser) {
  // parser->huart = huart;
  // parser->tim = tim;
  parser->rx_done = false;
  parser->idx = 0;
  parser->write_idx = 0;
  parser->first_packet = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
  // parser->tx_cb = tx_cb;

  // TODO: Move into some callback or something
  // HAL_UART_Receive_IT(parser->huart, &parser->rx_byte, 1);
}

void update_packet_parser_init(struct update_packet_parser_t *parser,
                               UART_HandleTypeDef *huart,
                               TIM_HandleTypeDef *tim,
                               void (*tx_cb)(const uint8_t *)) {
  parser->huart = huart;
  parser->tim = tim;
  parser->rx_done = false;
  parser->idx = 0;
  parser->write_idx = 0;
  parser->first_packet = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
  parser->tx_cb = tx_cb;

  // TODO: Move into some callback or something
  HAL_UART_Receive_IT(parser->huart, &parser->rx_byte, 1);
}

static void reset_buffer(struct update_packet_parser_t *parser) {
  parser->idx = 0;
}

static void packet_parser_check_rx_end(struct update_packet_parser_t *parser) {
  if (parser->write_idx == parser->fw_size) {
    /* i received everything so i can rearm erase flag to delete sector.
     * technically unneeded but ok */
    parser->first_packet = true;

    if (!update_packet_flash_validate_image_crc32(UPDATE_STORAGE_START_ADDR,
                                                  parser->fw_size)) {
      update_packet_parser_reset(parser);
      return;
    }

    // mark tx as done, rr system
    HAL_Delay(1000);
    custom_logger_log("Restarting system...\r\n");
    HAL_Delay(1000);
    HAL_NVIC_SystemReset();
  }
}

bool update_packet_parser_parse(struct update_packet_parser_t *parser) {

  if (!parser->rx_done) {
    return false;
  }

  parser->rx_done = false;

  uint16_t received_bytes = parser->idx;

  /* validate chunk size */
  if (!update_packet_validate_size(received_bytes, parser->write_idx,
                                   APP_MAX_SIZE)) {
    parser->tx_cb(&_NACK);
    reset_buffer(parser);

    return false;
  }

  /* validate chunk crc16 */
  if (!update_packet_validate_crc16(parser->buffer, received_bytes)) {
    parser->tx_cb(&_NACK);
    reset_buffer(parser);

    return false;
  }

  uint16_t payload_size = received_bytes - UPDATE_PACKET_OVERHEAD_SIZE;

  /* check if first packet so i can check the header */
  if (parser->first_packet) {
    /* erase flash sector 6 and 7 in this case */
    if (!update_packet_flash_erase_update()) {
      parser->tx_cb(&_NACK);
      reset_buffer(parser);

      return false;
    }

    if (!update_packet_validate_app_header(parser->buffer, &parser->fw_size)) {
      parser->tx_cb(&_NACK);
      reset_buffer(parser);

      return false;
    }

    /* i can receive the rest of the packets */
    parser->first_packet = false;
  }
  /* write received chunk to update sector */
  if (!update_packet_flash_write_chunk(
          UPDATE_STORAGE_START_ADDR + parser->write_idx,
          (const uint8_t *)parser->buffer, payload_size)) {
    parser->tx_cb(&_NACK);
    reset_buffer(parser);

    return false;
  }

  parser->write_idx += payload_size;
  reset_buffer(parser);
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
    HAL_TIM_Base_Stop_IT(parser->tim);
    if (parser->idx > UPDATE_PACKET_OVERHEAD_SIZE) {

      custom_logger_log("Received chunk size {%d}", parser->idx);
      parser->rx_done = true;
    } else {
      parser->tx_cb(&_NACK);
      parser->idx = 0;
    }
  }
}
