#ifndef OTW_PACKET_PARSER_H_
#define OTW_PACKET_PARSER_H_

#include <stdint.h>

#include "ring_buffer/ring_buffer.h"

#include "main.h"

#define MAX_PAYLOAD_SIZE 256

struct otw_uart_packet_t {
  uint8_t cmd;
  uint16_t length;
  uint8_t payload[MAX_PAYLOAD_SIZE];
  uint32_t crc;
};

enum otw_parse_state_t {
  OTW_PARSE_STATE_CMD,
  OTW_PARSE_STATE_LEN_LOW,
  OTW_PARSE_STATE_LEN_HIGH,
  OTW_PARSE_STATE_PAYLOAD,
  OTW_PARSE_STATE_CRC
};

enum otw_parse_result_t {
  OTW_PARSE_RESULT_BUSY,
  OTW_PARSE_RESULT_COMPLETE,
  OTW_PARSE_RESULT_ERROR,
};

struct otw_packet_parser_t {
  struct ring_buffer_t *rb;
  TIM_HandleTypeDef *timer;

  enum otw_parse_state_t state;
  struct otw_uart_packet_t packet;
  uint16_t payload_idx;
  uint8_t crc_buf[4];
  uint8_t crc_idx;
  volatile bool timeout;
  bool receiving;
};

void otw_packet_parser_init(struct otw_packet_parser_t *pp,
                            struct ring_buffer_t *rb, TIM_HandleTypeDef *tim);
void otw_packet_parser_reset(struct otw_packet_parser_t *pp);

void otw_packet_parser_timeout_callback(struct otw_packet_parser_t *pp,
                                        TIM_HandleTypeDef *tim);
enum otw_parse_result_t
otw_packet_parser_update(struct otw_packet_parser_t *pp);

const struct otw_uart_packet_t *
otw_packet_parser_get_packet(const struct otw_packet_parser_t *pp);

#endif // OTW_PACKET_PARSER_H_
