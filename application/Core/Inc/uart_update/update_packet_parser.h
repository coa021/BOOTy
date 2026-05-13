#ifndef UPDATE_PACKET_PARSER_H_
#define UPDATE_PACKET_PARSER_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* I want to send additional counter and end flag as header */
#define UPDATE_PACKET_HEADER_COUNTER_SIZE 4
#define UPDATE_PACKET_HEADER_END_SIZE 1
#define UPDATE_PACKET_HEADER_SIZE                                              \
  (UPDATE_PACKET_HEADER_COUNTER_SIZE + UPDATE_PACKET_HEADER_END_SIZE)

#define UPDATE_PACKET_CHUNK_SIZE 256
// length and crc16?
#define UPDATE_PACKET_CRC16_SIZE 2
#define UPDATE_PACKET_OVERHEAD_SIZE                                            \
  (UPDATE_PACKET_HEADER_SIZE + UPDATE_PACKET_CRC16_SIZE)
// 1 is for data
#define UPDATE_PACKET_MIN_SIZE (UPDATE_PACKET_OVERHEAD_SIZE + 1)
#define UPDATE_PACKET_BUFFER_SIZE                                              \
  (UPDATE_PACKET_CHUNK_SIZE + UPDATE_PACKET_OVERHEAD_SIZE)

#define UPDATE_PACKET_FIRST_PACKET_INDEX 1

#define UPDATE_PACKET_ACK 0x01
#define UPDATE_PACKET_NACK 0x15

struct update_packet_parser_t {
  UART_HandleTypeDef *huart;
  TIM_HandleTypeDef *tim;
  volatile uint8_t buffer[UPDATE_PACKET_BUFFER_SIZE];
  volatile uint8_t rx_byte;
  volatile uint16_t idx;
  volatile bool rx_done;

  uint32_t write_idx;

  uint32_t previous_counter;
  uint32_t current_counter;

  uint8_t tx_end;

  /* TODO: technically i would not need this now */
  // bool first_packet;
  uint32_t fw_size;

  void (*tx_cb)(const uint8_t *flag);
};

void update_packet_parser_init(struct update_packet_parser_t *parser,
                               UART_HandleTypeDef *huart,
                               TIM_HandleTypeDef *tim,
                               void (*tx_cb)(const uint8_t *));
bool update_packet_parser_parse_and_process(
    struct update_packet_parser_t *parser);

void update_packet_parser_uart_callback(struct update_packet_parser_t *parser,
                                        UART_HandleTypeDef *huart);

void update_packet_parser_tim_callback(struct update_packet_parser_t *parser,
                                       TIM_HandleTypeDef *htim);

#endif // UPDATE_PACKET_PARSER_H_
