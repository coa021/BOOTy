/**
 * @file bl_jump.c
 * @brief Bootloader application jump implementation.
 *
 * This module provides functionality for transferring execution from the
 * bootloader to the main application firmware.
 *
 */

#include "bl_jump.h"
#include "main.h"

#include "flash_layout.h"

/**
 * @brief Application entry function pointer type.
 *
 * Used to jump to the application's reset handler.
 */
typedef void (*p_func_t)(void);

/**
 * @brief Jump from bootloader to main application firmware.
 *
 * This function transfers execution control to the main application
 * located in flash memory.
 *
 * The application vector table is expected to be located at:
 * - `APP_START_ADDR`
 *
 */
void bl_jump_to_app(void) {
  uint32_t app_sp = *(volatile uint32_t *)(APP_START_ADDR);
  uint32_t app_reset = *(volatile uint32_t *)(APP_RESET_HANDLER_ADDRESS);

  __disable_irq();
  /* stopping systick */
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  /* testing if it makes difference here, obv not for now */
  SCB->VTOR = APP_START_ADDR;

  __enable_irq();
  __set_MSP(app_sp);

  p_func_t app_entry = (p_func_t)app_reset;
  app_entry();
}
