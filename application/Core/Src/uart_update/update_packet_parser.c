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

/**
 * @brief Reset packet parser state for the next package.
 *
 * This is used when, for example crc32 fails and we want to retransmit new
 * package, so we have to reset state of packet parser
 *
 *
 * @param parser parser of type struct update_packet_parser_t.
 * @return Nothing
 */
static void update_packet_parser_reset(struct update_packet_parser_t *parser) {
  parser->rx_done = false;
  parser->idx = 0;
  parser->write_idx = 0;
  parser->first_packet = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
}

/**
 * @brief Initialize packet parser.
 *
 * For the tx_cb, you have to pass a callback that will be executed for ACK and
 * NACK of packets. In my case i used UART IT
 *
 * @param parser Pointer to parser.
 * @param huart Pointer to huart.
 * @param tim Pointer to timer.
 * @param (*tx_cb) Pointer to function that receives const uint8_t*.
 * @param[in,out] buffer Description of a buffer used for both input and output.
 * @return Description of the return value (e.g., 0 for success, -1 for error).
 */
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

/**
 * @brief Reset buffer idx
 *
 *
 * @param parser Pointer to parser.
 * @return Nothing
 */
static void reset_buffer(struct update_packet_parser_t *parser) {
  parser->idx = 0;
}

/**
 * @brief Helper function. Check if we received last packet
 *
 * If we wrote in flash exactly the same size as firmware size, we can validate
 * crc32. On success it restarts the system. On fail it resets parser completely
 * so that we can receive new image
 *
 * @param parser Pointer to parser.
 * @return Nothing
 */
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

/**
 * @brief Parse and process received chunk of bytes
 *
 * When TIM callback stops the timer, rx done flag is set and this function is
 * executed. In here we are validating the size of chunk, chunk's crc16. If that
 * is ok and this is the first packet it can erase the update storage sectors
 * and it checks validity of the app header. If any of this fails, it returns
 * NACK. If everything is ok it stores the received chunk on FLASH.
 * If everything is ok, it sends back ACK and resets buffer. After that it
 * checks if we are at the end of the packet RX. If we are it validates crc32
 * and reboots system upon successful validation
 *
 *
 * @param parser Pointer to parser.
 * @return true/false depending on if all checks passed or not
 */
bool update_packet_parser_parse_and_process(
    struct update_packet_parser_t *parser) {

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

    if (!update_packet_validate_app_header(parser->buffer, &parser->fw_size)) {
      parser->tx_cb(&_NACK);
      reset_buffer(parser);

      return false;
    }

    /* grab firmware size */
    /* erase flash sector 6 (and 7 if needed) in this case */
    if (!update_packet_flash_erase_update(parser->fw_size)) {
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

  packet_parser_check_rx_end(parser);

  return true;
}

/**
 * @brief UART Callback.
 *
 * UART Interrupt. When we receive a byte on specified huart, it stops the
 * timer, fills the buffer with the received byte, and starts the timer. Like
 * this, timeout of timer is never triggered and we can keep on receiving bytes
 * untill we either have full chunk received or we receive remainder or whatever
 *
 * @param parser Pointer to parser
 * @param huart Pointer to huart
 * @return Nothing
 */
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

/**
 * @brief TIM Callback
 *
 * When timer expires (50ms timeout), interrupt is triggered. Stops the timer
 * and check if the buffer has minimal viable packet (at least 1 byte bigger
 * than packet overhead size). Marks the rx_done so that the packet can get
 * processed by the function in main loop (parse and process)
 *
 * @param *parser Parser of type struct update_packet_parser_t.
 * @param *htim Pointer to timer in TIM IT.
 * @return Nothing
 */
void update_packet_parser_tim_callback(struct update_packet_parser_t *parser,
                                       TIM_HandleTypeDef *htim) {

  if (htim->Instance == parser->tim->Instance) {
    HAL_TIM_Base_Stop_IT(parser->tim);
    if (parser->idx > UPDATE_PACKET_OVERHEAD_SIZE) {

      parser->rx_done = true;
    } else {
      parser->tx_cb(&_NACK);
      parser->idx = 0;
    }
  }
}
