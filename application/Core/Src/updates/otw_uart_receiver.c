#include "updates/otw_uart_receiver.h"

static struct otw_uart_receiver_t *_local_uart_receiver;

void otw_uart_receiver_init(struct otw_uart_receiver_t *ur,
                            struct ring_buffer_t *rb,
                            UART_HandleTypeDef *huart) {
  ur->huart = huart;
  ur->rb = rb;
  ur->rx_byte = 0;

  /* i cant access this in the uart calblack because of the fixed signature, so
   * im making local copy, but the actual thing is made from the main/controller
   * whatever */
  _local_uart_receiver = ur;

  HAL_UART_Receive_IT(ur->huart, &ur->rx_byte, 1);
}

void otw_uart_receiver_rx_cplt_callback(UART_HandleTypeDef *huart) {
  if (huart == _local_uart_receiver->huart) {
    rb_put(_local_uart_receiver->rb, _local_uart_receiver->rx_byte);
    HAL_UART_Receive_IT(_local_uart_receiver->huart,
                        &_local_uart_receiver->rx_byte, 1);
  }
}
