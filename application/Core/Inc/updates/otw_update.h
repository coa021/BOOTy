#ifndef OTW_UPDATE_H_
#define OTW_UPDATE_H_

#define OTW_FLAG_START 1

#include "stm32f4xx_hal.h"
#include "updates/otw_packet_parser.h"
#include <stdbool.h>

#define ACK 0x06
#define NACK 0x15

#define OTW_CMD_START 0x01
#define OTW_CMD_DATA 0x02
#define OTW_CMD_END 0x03

struct otw_update_t {
  UART_HandleTypeDef *huart;
  uint32_t expected_fw_size;
  uint32_t bytes_written;
};

void otw_update_init(struct otw_update_t *update, UART_HandleTypeDef *huart);

bool otw_update_handle_packet(struct otw_update_t *update,
                              const struct otw_uart_packet_t *packet);

void otw_update_handle_error(struct otw_update_t *update);

#endif // OTW_UPDATE_H_
