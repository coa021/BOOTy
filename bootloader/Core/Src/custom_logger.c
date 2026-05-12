#include "custom_logger.h"
#include <string.h>

#include <stdarg.h>
#include <stdio.h>

static UART_HandleTypeDef *logger_huart;

int custom_logger_init(UART_HandleTypeDef *huart1){
  if (!logger_huart) {
    logger_huart = huart1;
  }
    return 0;
}

void custom_logger_log(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  HAL_UART_Transmit(logger_huart, (uint8_t *)buf, (uint16_t)strlen(buf), 100);
}
