#include "updates/otw_update.h"
#include "main.h"

#include "flash/operations.h"
#include "flash_layout.h"

#define FLASH_WRITE_CHUNK 256

static uint8_t fw_buf[FLASH_WRITE_CHUNK];
static uint32_t fw_buf_idx = 0;
static uint32_t flash_write_addr = UPDATE_STORAGE_START_ADDR;

void otw_update_init(void) {
//   Flash_Erase_AppSlot(); // erase once before anything arrives

  fw_buf_idx = 0;
  flash_write_addr = UPDATE_STORAGE_START_ADDR;
}

void otw_update_receive_byte(uint8_t byte) {
  fw_buf[fw_buf_idx++] = byte;

  if (fw_buf_idx == FLASH_WRITE_CHUNK) {
    // buffer full,  to flash
    Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf,
                     FLASH_WRITE_CHUNK / 4);
    flash_write_addr += FLASH_WRITE_CHUNK;
    fw_buf_idx = 0;
  }
}

void otw_update_flush(void)
{
    if (fw_buf_idx == 0) {
        return;
    }

    while (fw_buf_idx % 4 != 0) {
        fw_buf[fw_buf_idx++] = 0xFF;
    }

    Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf, fw_buf_idx / 4);
    fw_buf_idx = 0;
}
