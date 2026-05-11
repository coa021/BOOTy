#include "uart_update/update_packet_parser.h"

#include "custom_crc/custom_crc32.h"
#include "custom_logger.h"

#include <string.h>

void update_packet_parser_init(struct update_packet_parser_t *parser,
                               UART_HandleTypeDef *huart,
                               TIM_HandleTypeDef *tim, void (*cb_ack)(void),
                               void (*cb_nack)(void)) {
  parser->huart = huart;
  parser->tim = tim;
  parser->rx_done = false;
  parser->idx = 0;
  /*  void *memset(size_t n;
                  void s[n], int c, size_t n);

DESCRIPTION
     The memset() function fills the first n bytes of the memory area pointed to
by s with the constant byte c. */
  memset(parser->buffer, 0, UPDATE_PACKET_BUFFER_SIZE);
  parser->cb_ack = cb_ack;
  parser->cb_nack = cb_nack;

  // TODO: Move into some callback or something
  HAL_UART_Receive_IT(parser->huart, &parser->rx_byte, 1);
}

bool update_packet_parser_parse(struct update_packet_parser_t *parser) {
  if (parser->rx_done) {
    parser->rx_done = false;

    custom_logger_log("Parser done let me print\r\n");
    parser->buffer[parser->idx] = '\0';
    custom_logger_log("Message: %s", parser->buffer);
    uint16_t calculated_crc16 = crc16(parser->buffer, parser->idx - 2);
    uint16_t expected_crc16 = parser->buffer[parser->idx - 2] |
                              (parser->buffer[parser->idx - 1] << 8);

    parser->idx = 0;
    parser->buffer[0] = '\0';

       if (calculated_crc16 != expected_crc16) {
         custom_logger_log("Missmatch in crc16; expected: %d,\tactual:
         %d\r\n",
                           expected_crc16, calculated_crc16);
         parser->cb_nack();
         return false;
       }

    custom_logger_log("Crc16 is matching\r\n");

    parser->cb_ack();

    return true;
  }
  return false;
}

void update_packet_parser_uart_callback(struct update_packet_parser_t *parser,
                                        UART_HandleTypeDef *huart) {
  if (huart->Instance == parser->huart->Instance) {
    // TODO: Create callbacks for all this to separate HAL from APP later
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
    // GT 1 because i have to include crc i guess
    if (parser->idx > 1) {
      // i received whole chunk i need to tell timer to print it by disabling it
      // HAL_UART_Transmit_IT(&huart1, &_ACK, 1);
      parser->rx_done = true;
    } else {
      // custom_logger_log("Nack\r\n");
      // HAL_UART_Transmit_IT(&huart1, &_NACK, 1);
      parser->cb_nack();
      parser->idx = 0;
    }
  }
}
