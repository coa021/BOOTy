#include "operations.h"
#include "stm32f4xx_hal.h"


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

int32_t Flash_Erase_Sectors(uint32_t sector, uint32_t num_sectors)
{
  /* i need some check if it is already removed or something similar */
    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        .Sector       = sector,
        .NbSectors    = num_sectors,
        // from sector 2 to 6 i guess? but not including 6
    };

    uint32_t sector_error;
    HAL_FLASH_Unlock();
    uint32_t result = (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
                      ? HAL_FLASH_GetError()
                      : 0;
    HAL_FLASH_Lock();
    return result;
}

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