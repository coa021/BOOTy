#include "custom_logger.h"
#include <string.h>

#include <stdarg.h>
#include <stdio.h>

static UART_HandleTypeDef *_logger_huart;

int custom_logger_init(UART_HandleTypeDef *huart) {
  if (!_logger_huart) {
    _logger_huart = huart;
  }
  return 0;
}

void custom_logger_log(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  HAL_UART_Transmit_IT(_logger_huart, (uint8_t *)buf, (uint16_t)strlen(buf));
}
