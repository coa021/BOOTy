/**
 * @file operations.c
 * @brief Low-level FLASH memory operations for STM32F411 bootloader.
 *
 * Provides:
 * - Sector address resolution
 * - Flash word programming
 * - Sector erase operations
 * - Flash read operations
 *
 */

#include "operations.h"
#include "stm32f4xx_hal.h"

/**
 * @brief Get flash sector index from a memory address.
 *
 * Maps a raw flash memory address to its corresponding STM32F411 sector.
 *
 * Address ranges:
 * - Sector 0: 0x08000000 - 0x08004000
 * - Sector 1: 0x08004000 - 0x08008000
 * - Sector 2: 0x08008000 - 0x0800C000
 * - Sector 3: 0x0800C000 - 0x08010000
 * - Sector 4: 0x08010000 - 0x08020000
 * - Sector 5: 0x08020000 - 0x08040000
 * - Sector 6: 0x08040000 - 0x08060000
 *
 * @param Address Flash memory address.
 *
 * @return Corresponding FLASH sector number.
 *
 * @note Only supports STM32F4 sector layout.
 */
static uint32_t GetSector(uint32_t Address) {
  uint32_t sector = 0;

  if ((Address >= 0x08000000) && (Address < 0x08004000)) {
    sector = FLASH_SECTOR_0;
  }

  else if ((Address >= 0x08004000) && (Address < 0x08008000)) {
    sector = FLASH_SECTOR_1;
  }

  else if ((Address >= 0x08008000) && (Address < 0x0800C000)) {
    sector = FLASH_SECTOR_2;
  }

  else if ((Address >= 0x0800C000) && (Address < 0x08010000)) {
    sector = FLASH_SECTOR_3;
  }

  else if ((Address >= 0x08010000) && (Address < 0x08020000)) {
    sector = FLASH_SECTOR_4;
  }

  else if ((Address >= 0x08020000) && (Address < 0x08040000)) {
    sector = FLASH_SECTOR_5;
  }

  else if ((Address >= 0x08040000) && (Address < 0x08060000)) {
    sector = FLASH_SECTOR_6;
  }

  return sector;
}

/**
 * @brief Write 32-bit words to flash memory.
 *
 * Programs flash memory sequentially starting at the given address.
 *
 * The function:
 * - Unlocks flash memory
 * - Writes data word-by-word (32-bit)
 * - Advances address automatically
 * - Locks flash after completion
 *
 * @param StartSectorAddress Destination flash address.
 * @param Data Pointer to data buffer (32-bit words).
 * @param numberofwords Number of 32-bit words to write.
 *
 * @retval 0 on success
 * @retval HAL flash error code on failure
 */
uint32_t Flash_Write_Data(uint32_t StartSectorAddress, uint32_t *Data,
                          uint16_t numberofwords) {
  uint32_t SECTORError;
  int sofar = 0;

  HAL_FLASH_Unlock();

  while (sofar < numberofwords) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, StartSectorAddress,
                          Data[sofar]) == HAL_OK) {
      StartSectorAddress += 4;
      sofar++;
    } else {
      return HAL_FLASH_GetError();
    }
  }

  HAL_FLASH_Lock();

  return 0;
}

/**
 * @brief Erase one or more flash sectors.
 *
 * Performs a sector-based erase operation using STM32 HAL.
 *
 * @param sector Starting sector number.
 * @param num_sectors Number of sectors to erase.
 *
 * @retval 0 on success
 * @retval HAL flash error code on failure
 */
int32_t Flash_Erase_Sectors(uint32_t sector, uint32_t num_sectors) {
  FLASH_EraseInitTypeDef erase = {
      .TypeErase = FLASH_TYPEERASE_SECTORS,
      .VoltageRange = FLASH_VOLTAGE_RANGE_3,
      .Sector = sector,
      .NbSectors = num_sectors,
  };

  uint32_t sector_error;
  HAL_FLASH_Unlock();
  uint32_t result = (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
                        ? HAL_FLASH_GetError()
                        : 0;
  HAL_FLASH_Lock();
  return result;
}

/**
 * @brief Read 32-bit words from flash memory.
 *
 * Copies raw flash contents into a RAM buffer.
 *
 * @param StartSectorAddress Source flash address.
 * @param RxBuf Destination buffer.
 * @param numberofwords Number of 32-bit words to read.
 */
void Flash_Read_Data(uint32_t StartSectorAddress, uint32_t *RxBuf,
                     uint16_t numberofwords) {
  while (1) {
    *RxBuf = *(__IO uint32_t *)StartSectorAddress;
    StartSectorAddress += 4;
    RxBuf++;
    if (!(numberofwords--))
      break;
  }
}