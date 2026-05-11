#ifndef INC_CUSTOM_LOGGER_H_
#define INC_CUSTOM_LOGGER_H_

#include "main.h"

int custom_logger_init(UART_HandleTypeDef *huart1);

void custom_logger_log(const char *fmt, ...);

#endif
