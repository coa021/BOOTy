#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#include <stdint.h>

uint32_t Flash_Write_Data(uint32_t StartSectorAddress, uint32_t *Data, uint16_t numberofwords);
void Flash_Read_Data(uint32_t StartSectorAddress, uint32_t *RxBuf, uint16_t numberofwords);
int32_t Flash_Erase_Sectors(uint32_t sector, uint32_t num_sectors);

#endif //OPERATIONS_H_
