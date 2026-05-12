#ifndef OTW_UART_RECEIVER_H_
#define OTW_UART_RECEIVER_H_

#include "ring_buffer/ring_buffer.h"
#include "stm32f4xx_hal.h"

struct otw_uart_receiver_t {
  UART_HandleTypeDef *huart;
  struct ring_buffer_t *rb;
  uint8_t rx_byte;
};

void otw_uart_receiver_init(struct otw_uart_receiver_t *ur,
                            struct ring_buffer_t *rb,
                            UART_HandleTypeDef *huart);

void otw_uart_receiver_rx_cplt_callback(UART_HandleTypeDef *huart);

#endif // OTW_UART_RECEIVER_H_
