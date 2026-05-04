#include "custom_logger.h"
#include <string.h>

static UART_HandleTypeDef *huart;

int custom_logger_init(UART_HandleTypeDef *huart1){
    if(!huart){
        huart = huart1;
    }
    return 0;
}

void custom_logger_log(const char *msg){
  HAL_UART_Transmit(huart, (uint8_t *)msg, (uint16_t)strlen(msg), 100);
    
}