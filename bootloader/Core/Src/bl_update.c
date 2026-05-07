#include "bl_update.h"

#include "bl_verify.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include "app_header.h"
#include <string.h>


#define COPY_BUFFER_SIZE 256

/* HELPERS--------------------------------------- */
static bool fw_copy_to_address(uint32_t copy_start_addr,
                               uint32_t write_start_addr,
                               uint32_t copy_app_size) {
  uint8_t *src = (uint8_t *)copy_start_addr;
  uint8_t fw_buf[COPY_BUFFER_SIZE];
  uint32_t bytes_copied = 0;
  uint32_t fw_app_size = copy_app_size + APP_HEADER_SIZE;
  uint32_t flash_write_addr = write_start_addr;
  int32_t chunk;
  uint32_t words;
  uint32_t ret;

  while (bytes_copied < fw_app_size) {
    chunk = (fw_app_size - bytes_copied) < COPY_BUFFER_SIZE
                ? (fw_app_size - bytes_copied)
                : COPY_BUFFER_SIZE;
    memset(fw_buf, 0xFF,
           COPY_BUFFER_SIZE); /* setting it to fixed 256 in this case */
    memcpy(fw_buf, src + bytes_copied,
           chunk); /* copy even if we have less than 256, rest will be padded
                      with 0xff like when u flash erase a sector */
    /* (n + d - 1) / d => i want to round chunk to 4 with this formula */
    words = (chunk + 3) / 4;
    ret = Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf, words);
    if (ret != HAL_FLASH_ERROR_NONE) {
      custom_logger_log("Error in Flash_Write_Data with ret: %d", ret);
      return false;
    }
    /* we wrote full words, nothing is leftover */
    flash_write_addr += words * 4;
    bytes_copied += chunk;
  }

  custom_logger_log("fw_copy_to_sector succeeded in copying app");
  return true;
}

/* funtions --------------------------------------- */
bool bl_check_for_update(void) {
  custom_logger_log("Current app version is: %x", get_app_header()->version);

  custom_logger_log("Current update_app_header version is: %x",
                    get_update_header()->version);

  return (get_update_header()->version >= get_app_header()->version);
}

bool bl_swap_updates(void) {
  /* verify if the app from update is valid */
  enum verify_result_t res = bl_verify_app(get_update_header());
  if (res != VERIFY_OK) {
    custom_logger_log(
        "[BL]: Failed verifying updated firmware app with reason: %d\r\n", res);
    return false;
  }

  /* erase sector 2 to sector 6, this is main app */
  Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
  /* i need some way to move chunks from update slot to main app slot */
  bool ret = fw_copy_to_address(UPDATE_STORAGE_START_ADDR, APP_HEADER_ADDR,
                                get_update_header()->size);
  if (!ret) {
    custom_logger_log("fw_copy_to_address failed in copying\r\n");
  }
  //   uint8_t *src = (uint8_t *)UPDATE_STORAGE_START_ADDR;
  //   uint8_t fw_buf[COPY_BUFFER_SIZE];
  //   uint32_t bytes_copied = 0;
  //   uint32_t fw_size = get_update_header()->size + APP_HEADER_SIZE;
  //   uint32_t flash_write_addr = APP_HEADER_ADDR;
  //   int32_t chunk;

  //   while (bytes_copied < fw_size) {
  //     chunk = (fw_size - bytes_copied) < COPY_BUFFER_SIZE ? (fw_size -
  //     bytes_copied) : COPY_BUFFER_SIZE; memcpy(fw_buf, src + bytes_copied,
  //     chunk); Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf, chunk /
  //     4); flash_write_addr += chunk; bytes_copied += chunk;
  //   }

  return true;
  //
}

bool bl_swap_partitions(void) {

  /* check if the update fw is valid */
  enum verify_result_t res = bl_verify_app(get_update_header());
  if (res != VERIFY_OK) {
    custom_logger_log(
        "[BL]: Failed verifying updated firmware app with reason: %d\n", res);
    return false;
  }

  /* i need to grab the fw into ram, overwrite it with the main app, move update
   * from ram to main slot */
  const uint32_t fw_update_size = get_update_header()->size + 512;
  uint8_t fw_update_buff[fw_update_size];
  memcpy(fw_update_buff, get_update_header(), fw_update_size);

  /* remove update fw sectors */
  Flash_Erase_Sectors(FLASH_SECTOR_6, 1);
  /* move the main app to the update sector */
  bool ret = fw_copy_to_address(APP_HEADER_ADDR, UPDATE_STORAGE_START_ADDR,
                                get_app_header()->size);
  if (!ret) {
    custom_logger_log("Error copying main app to new address, exiting\r\n");
    return false;
  }

  /* move update to main app's section */
  Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
  ret = fw_copy_to_address((uint32_t)fw_update_buff, APP_HEADER_ADDR,
                           fw_update_size);

  if (!ret) {
    custom_logger_log(
        "Error copying copied firmware from ram to new address, exiting\r\n");
    return false;
  }

  /* TODO: this will brick the device if some error happens in between lol, very
   * unsafe */
  return true;
}
