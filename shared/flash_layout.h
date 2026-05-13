/**
 * @file flash_layout.h
 * @brief Flash memory layout definitions and address mapping.
 *
 * This header defines the flash memory regions used by:
 * - Bootloader (BL)
 * - Application (APP)
 * - Application header
 * - Update storage region
 *
 * The layout is derived from linker script symbols and is used to
 * ensure consistent memory addressing across bootloader and firmware.
 */

#ifndef _SHARED__FLASH_LAYOUT_H
#define _SHARED__FLASH_LAYOUT_H

#include <stdint.h>

/* Grabbing values from the linker script */
extern uint32_t __APP_FLASH_START;            // 0x08008200
extern uint32_t __APP_HEADER_START;           // 0x08008000
extern uint32_t __APP_MAIN_LENGTH;            // 224K
extern uint32_t __BL_LENGTH;                  // 32K
extern uint32_t __UPDATE_STORAGE_FLASH_START; // 0x08040000
extern uint32_t __UPDATE_STORAGE_LENGTH;      // 256K

/**
 * @brief Bootloader start address.
 *
 * @note Defined via linker symbol __BL_FLASH_START
 */
#define BL_START_ADDR ((uint32_t)&__BL_FLASH_START)
/**
 * @brief Bootloader size in bytes.
 *
 */
#define BL_SIZE ((uint32_t)&__BL_LENGTH)

/**
 * @brief Application header base address.
 */
#define APP_HEADER_ADDR ((uint32_t)&__APP_HEADER_START)

/* sector 2-5 */
/**
 * @brief Application code start address.
 */
#define APP_START_ADDR ((uint32_t)&__APP_FLASH_START)

/**
 * @brief Maximum allowed application size.
 */
#define APP_MAX_SIZE ((uint32_t)&__APP_MAIN_LENGTH)

/**
 * @brief Size of application header region.
 *
 * Calculated as the gap between header start and application start.
 */
#define APP_HEADER_SIZE (APP_START_ADDR - APP_HEADER_ADDR)

/**
 * @brief Reset handler address inside application vector table.
 *
 * Cortex-M vector table layout:
 * - Offset 0x00: Initial MSP
 * - Offset 0x04: Reset handler
 */
#define APP_RESET_HANDLER_ADDRESS (APP_START_ADDR + 4U)

/**
 * @brief Update storage region start address.
 */
#define UPDATE_STORAGE_START_ADDR ((uint32_t)&__UPDATE_STORAGE_FLASH_START)

/**
 * @brief Update storage region size in bytes.
 */
#define UPDATE_STORAGE_SIZE ((uint32_t)&__UPDATE_STORAGE_LENGTH)

/**
 * @brief Application start address inside update storage region.
 *
 * This accounts for the header offset inside update flash.
 */
#define UPDATE_STORAGE_APP_START_ADDR                                          \
  (UPDATE_STORAGE_START_ADDR + APP_HEADER_SIZE)

/**
 * @brief Maximum firmware size supported in update storage.
 */
#define UPDATE_STORAGE_MAX_SIZE APP_MAX_SIZE

#endif
