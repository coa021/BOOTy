#include "bl_update.h"

#include "bl_verify.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include "app_header.h"
#include <string.h>


#define COPY_BUFFER_SIZE 256


bool bl_check_for_update(void) {
  custom_logger_log("Current app version is: %x", get_app_header()->version);

  custom_logger_log("Current update_app_header version is: %x",
                    get_update_header()->version);

  return (get_update_header()->version > get_app_header()->version);
}

bool bl_swap_updates(void) {
  /* verify if the app from update is valid */
  enum verify_result_t res = bl_verify_app(get_update_header());
  if (res != VERIFY_OK) {
    custom_logger_log(
        "[BL]: Failed verifying updated firmware app with reason: %d\n", res);
    return false;
  }

  /* erase sector 2 to sector 6, this is main app */
  Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
  /* i need some way to move chunks from update slot to main app slot */
  uint8_t *src = (uint8_t *)UPDATE_STORAGE_START_ADDR;
  uint8_t fw_buf[COPY_BUFFER_SIZE];
  uint32_t bytes_copied = 0;
  uint32_t fw_size = get_update_header()->size + APP_HEADER_SIZE;
  uint32_t flash_write_addr = APP_HEADER_ADDR;
  int32_t chunk;

  while (bytes_copied < fw_size) {
    chunk = (fw_size - bytes_copied) < COPY_BUFFER_SIZE ? (fw_size - bytes_copied) : COPY_BUFFER_SIZE;
    memcpy(fw_buf, src + bytes_copied, chunk);
    Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf, chunk / 4);
    flash_write_addr += chunk;
    bytes_copied += chunk;
  }

  custom_logger_log("[BL]: swap completed with %u bytes copied, fw size: %u", bytes_copied, fw_size);
  return true;
  //
}
