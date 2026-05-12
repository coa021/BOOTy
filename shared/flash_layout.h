#ifndef _SHARED__FLASH_LAYOUT_H
#define _SHARED__FLASH_LAYOUT_H

#include <stdint.h>

/*
Now regarding sectors of the main memory, this is how it looks like
Sector 0 0x0800 0000 - 0x0800 3FFF 16 Kbytes
Sector 1 0x0800 4000 - 0x0800 7FFF 16 Kbytes
Sector 2 0x0800 8000 - 0x0800 BFFF 16 Kbytes
Sector 3 0x0800 C000 - 0x0800 FFFF 16 Kbytes
Sector 4 0x0801 0000 - 0x0801 FFFF 64 Kbytes
Sector 5 0x0802 0000 - 0x0803 FFFF 128 Kbytes
Sector 6 0x0804 0000 - 0x0805 FFFF 128 Kbytes
Sector 7 0x0806 0000 - 0x0807 FFFF 128 Kbytes

Lets say sector 0 will be bootloader
sector 1 will be app header
sector 2-5 will be main app
sector 6-7 will be where i store OTA updated firmware
A bit illogical, update space is larger than main app state but this is
just an example
 */
/* #define FLASH_SECTOR_0_START 0x08000000U
#define FLASH_SECTOR_1_START 0x08004000U
#define FLASH_SECTOR_2_START 0x08008000U
#define FLASH_SECTOR_3_START 0x0800C000U
#define FLASH_SECTOR_4_START 0x08010000U
#define FLASH_SECTOR_5_START 0x08020000U
#define FLASH_SECTOR_7_START 0x08060000U */

// #define FLASH_SECTOR_6_START 0x08040000U
// #define FLASH_SECTOR_7_END 0x0807FFFFU

/* Grabbing values from the linker script */
extern uint32_t __APP_FLASH_START;            // 0x08008200
extern uint32_t __APP_HEADER_START;           // 0x08008000
extern uint32_t __APP_MAIN_LENGTH;            // 224K
extern uint32_t __BL_LENGTH;                  // 32K
extern uint32_t __UPDATE_STORAGE_FLASH_START; // 0x08040000
extern uint32_t __UPDATE_STORAGE_LENGTH;      // 256K

/* sector 0 and 1*/
#define BL_START_ADDR ((uint32_t)&__BL_FLASH_START)
#define BL_SIZE ((uin32_t) & __BL_LENGTH)

/* sector 2 */
#define APP_HEADER_ADDR ((uint32_t)&__APP_HEADER_START)
// #define APP_HEADER_SECTOR       FLASH_SECTOR_1_START

/* sector 2-5 */
#define APP_START_ADDR ((uint32_t)&__APP_FLASH_START)
// #define APP_START_SECTOR        FLASH_SECTOR_2_START
#define APP_MAX_SIZE ((uint32_t)&__APP_MAIN_LENGTH)
//       (FLASH_SECTOR_6_START - FLASH_SECTOR_2_START)

#define APP_HEADER_SIZE (APP_START_ADDR - APP_HEADER_ADDR)

/* sector 6 and 7 */

#define UPDATE_STORAGE_START_ADDR ((uint32_t)&__UPDATE_STORAGE_FLASH_START)
#define UPDATE_STORAGE_SIZE ((uin32_t) & __UPDATE_STORAGE_LENGTH)
#define UPDATE_STORAGE_APP_START_ADDR                                          \
  (UPDATE_STORAGE_START_ADDR + APP_HEADER_SIZE)

#define UPDATE_STORAGE_MAX_SIZE APP_MAX_SIZE

/* sector 6 and 7 */
// #define OTA_UPDATE_START_ADDR   0x08040000U
// #define OTA_UPDATE_START_SECTOR FLASH_SECTOR_6_START
// #define OTA_UPDATE_MAX_SIZE     (FLASH_SECTOR_7_END - FLASH_SECTOR_6_START)

/* first 4 bytes are initial stack pointer, i need reset handler which comes
 * after that */
#define APP_RESET_HANDLER_ADDRESS (APP_START_ADDR + 4U)

#endif
