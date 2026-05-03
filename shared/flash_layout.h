#ifndef _SHARED__FLASH_LAYOUT_H
#define _SHARED__FLASH_LAYOUT_H

#include "app_header.h"

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
TODO: A bit illogical, update space is larger than main app state but this is just an example
 */
/* idk if what im doing is right lol
TODO: Check on this logic */
#define FLASH_SECTOR_0_START 0x08000000U
#define FLASH_SECTOR_1_START 0x08004000U
#define FLASH_SECTOR_2_START 0x08008000U
#define FLASH_SECTOR_3_START 0x0800C000U
#define FLASH_SECTOR_4_START 0x08010000U
#define FLASH_SECTOR_5_START 0x08020000U
#define FLASH_SECTOR_6_START 0x08040000U
#define FLASH_SECTOR_7_START 0x08060000U


#define FLASH_SECTOR_7_END   0x0807FFFFU

/* sector 0 */
#define BL_START_ADDR           FLASH_SECTOR_0_START
#define BL_SIZE                 (16U * 1024U)

/* sector 1 */
#define APP_HEADER_ADDR         0x08004000U
#define APP_HEADER_SECTOR       FLASH_SECTOR_1_START

/* sector 2-5 */
#define APP_START_ADDR          0x08008000U
#define APP_START_SECTOR        FLASH_SECTOR_2_START
#define APP_MAX_SIZE            (FLASH_SECTOR_6_START - FLASH_SECTOR_2_START)

/* sector 6 and 7 */
#define OTA_UPDATE_START_ADDR   0x08040000U
#define OTA_UPDATE_START_SECTOR FLASH_SECTOR_6_START
#define OTA_UPDATE_MAX_SIZE     (FLASH_SECTOR_7_END - FLASH_SECTOR_6_START)

#endif