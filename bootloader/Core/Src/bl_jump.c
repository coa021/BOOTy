#include "bl_jump.h"
#include "main.h"

#include "flash_layout.h"

typedef void (*p_func_t)(void);

void bl_jump_to_app(void) {
  uint32_t app_sp = *(volatile uint32_t *)(APP_START_ADDR);
  uint32_t app_reset = *(volatile uint32_t *)(APP_RESET_HANDLER_ADDRESS);

  __disable_irq();
  /* stopping systick */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  __set_MSP(app_sp);

  p_func_t app_entry = (p_func_t)app_reset;
  app_entry();
}
