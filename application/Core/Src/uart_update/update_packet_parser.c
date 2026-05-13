/**
 * @file update_packet_parser.c
 * @brief UART firmware update packet parser implementation.
 *
 * This module implements the UART firmware update packet receiver and parser.
 *
 * Responsibilities include:
 * - Receiving UART update packets
 * - Detecting packet boundaries using timer timeouts
 * - Validating packet ordering
 * - Validating packet CRC16 integrity
 * - Validating firmware application headers
 * - Writing firmware chunks into flash memory
 * - Sending ACK/NACK responses
 * - Detecting transfer completion
 * - Triggering final firmware CRC32 validation
 * - Restarting the system after successful firmware upload
 *
 * The parser operates using:
 * - UART interrupt callbacks for byte reception
 * - Timer interrupt callbacks for packet timeout detection
 * - Main loop processing for packet validation and flash writes
 */

#include "uart_update/update_packet_parser.h"

#include "app_header.h"
#include "custom_crc/custom_crc.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include <string.h>

#include "uart_update/update_packet_flash.h"
#include "uart_update/update_packet_validator.h"

/**
 * @brief ACK response byte sent after successful packet processing.
 */
static const uint8_t _ACK = UPDATE_PACKET_ACK;

/**
 * @brief NACK response byte sent after failed packet validation.
 */
static const uint8_t _NACK = UPDATE_PACKET_NACK;

/**
 * @brief Reset parser state for a new firmware transfer.
 *
 * Clears parser runtime state and receive buffer contents.
 *
 * @param parser Pointer to update packet parser instance.
 */
static void update_packet_parser_reset(struct update_packet_parser_t *parser) {
  parser->rx_done = false;
  parser->idx = 0;
  parser->write_idx = 0;
  //  parser->first_packet = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
}

/**
 * @brief Initialize firmware update packet parser.
 *
 * Initializes parser runtime state and configures:
 * - UART handle
 * - Timer handle
 * - ACK/NACK transmit callback
 * - Receive buffer state
 *
 * UART reception is started immediately using interrupt mode.
 *
 * @param parser Pointer to parser instance.
 * @param huart Pointer to UART handle.
 * @param tim Pointer to timer handle.
 * @param tx_cb Callback used to transmit ACK/NACK responses.
 *
 * @note UART reception uses single-byte interrupt mode.
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
  //  parser->first_packet = true;
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
  parser->tx_cb = tx_cb;
  /* TODO */
  parser->previous_counter = parser->current_counter = 0;
  parser->tx_end = 0;

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
 * @brief Check whether the firmware transfer is complete.
 *
 * When the transfer completion flag is received:
 * - Validate the final firmware image CRC32
 * - Reset parser state on failure
 * - Restart the MCU on success
 *
 * @param parser Pointer to parser instance.
 *
 * @note Final firmware validation is performed only after all packets
 *       are successfully written into flash.
 */
static void packet_parser_check_rx_end(struct update_packet_parser_t *parser) {
  // if (parser->write_idx == parser->fw_size)
  if (parser->tx_end) {
    custom_logger_log("\nfw size: {%d}\r\n", parser->fw_size);
    /* i received everything so i can rearm erase flag to delete sector.
     * technically unneeded but ok */
    // parser->first_packet = true;

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
 * @brief Parse and process a fully received firmware packet.
 *
 * Processing stages:
 * 1. Validate packet ordering
 * 2. Validate packet size
 * 3. Validate packet CRC16
 * 4. Validate firmware application header (first packet only)
 * 5. Erase update flash sectors (first packet only)
 * 6. Write firmware payload into flash
 * 7. Send ACK or NACK response
 * 8. Detect end of firmware transfer
 *
 * @param parser Pointer to parser instance.
 *
 * @retval true  Packet processed successfully.
 * @retval false Packet validation or flash operation failed.
 *
 * @note This function is intended to run from the main loop after
 *       packet reception is completed.
 */
bool update_packet_parser_parse_and_process(
    struct update_packet_parser_t *parser) {

  if (!parser->rx_done) {
    return false;
  }

  parser->rx_done = false;

  /* grab the current counter, check if its valid */
  memcpy(&parser->current_counter, parser->buffer, sizeof(uint32_t));
  /* grab last packet end flag */
  memcpy(&parser->tx_end, parser->buffer + sizeof(parser->current_counter),
         sizeof(uint8_t));
  custom_logger_log("Counter is: {%d}. End flag is: {%d}\r",
                    parser->current_counter, parser->tx_end);

  if ((parser->previous_counter + 1) != parser->current_counter) {
    custom_logger_log("Chunk order missmatch! Previous counter: {%d}. Current "
                      "counter: {%d}\r\n",
                      parser->previous_counter, parser->current_counter);

    /* chunk order missmatch i want to nack that or should i simply
     * terminate/timeout the communication? */
    reset_buffer(parser);
    parser->tx_cb(&_NACK);
    return false;
  }

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

  /* TODO: I need to change whole logic. i have counter now, so if it is the
   * first package i need to tell it that */
  /* check if first packet so i can check the header */
  // if (parser->first_packet) {
  if (parser->current_counter == UPDATE_PACKET_FIRST_PACKET_INDEX) {
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
    // parser->first_packet = false;
    /* Increment it, but what will happen to previous_packet? i need some
     * explicit check here */
    // ++parser->current_packet;
  }
  /* write received chunk to update sector */
  if (!update_packet_flash_write_chunk(
          UPDATE_STORAGE_START_ADDR + parser->write_idx,
          (const uint8_t *)(parser->buffer + UPDATE_PACKET_HEADER_SIZE),
          payload_size)) {
    parser->tx_cb(&_NACK);
    reset_buffer(parser);

    return false;
  }

  parser->write_idx += payload_size;

  /* everything ok now i can set the previous chunk counter to current one */
  parser->previous_counter = parser->current_counter;
  reset_buffer(parser);
  parser->tx_cb(&_ACK);

  packet_parser_check_rx_end(parser);

  return true;
}

/**
 * @brief UART receive interrupt callback.
 *
 * Handles byte-by-byte UART reception.
 *
 * Operation:
 * - Stops timeout timer
 * - Stores received byte into parser buffer
 * - Rearms UART reception interrupt
 * - Restarts timeout timer
 *
 * This mechanism allows packets to be delimited using
 * inactivity timeout detection.
 *
 * @param parser Pointer to parser instance.
 * @param huart Pointer to UART handle.
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
 * @brief Timer timeout interrupt callback.
 *
 * Triggered when no UART bytes are received within the configured
 * timeout period.
 *
 * This marks the end of a received packet.
 *
 * Behavior:
 * - Stops timeout timer
 * - Marks packet reception complete if packet size is valid
 * - Sends NACK for packets smaller than minimum overhead size
 *
 * @param parser Pointer to parser instance.
 * @param htim Pointer to timer handle.
 */
void update_packet_parser_tim_callback(struct update_packet_parser_t *parser,
                                       TIM_HandleTypeDef *htim) {

  if (htim->Instance == parser->tim->Instance) {
    HAL_TIM_Base_Stop_IT(parser->tim);
    if (parser->idx > UPDATE_PACKET_OVERHEAD_SIZE) {
      // custom_logger_log("Received chunk size: %d", parser->idx);
      parser->rx_done = true;
    } else {
      parser->tx_cb(&_NACK);
      parser->idx = 0;
    }
  }
}
