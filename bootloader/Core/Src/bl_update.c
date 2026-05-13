/**
 * @file bl_update.c
 * @brief Bootloader firmware update management implementation.
 *
 * This module provides functionality for:
 * - Detecting available firmware updates
 * - Verifying update firmware integrity
 * - Copying firmware images between flash regions
 * - Applying firmware updates to the main application slot
 * - Clearing the update storage sector
 *
 * The update flow is:
 * 1. Check if a newer firmware exists in the update slot
 * 2. Verify the update image integrity
 * 3. Erase the main application sectors
 * 4. Copy the update image into the main application slot
 * 5. Optionally clear the update slot after successful update
 */

#include "bl_update.h"

#include "app_header.h"
#include "bl_verify.h"
#include "custom_logger.h"
#include "flash/operations.h"
#include "flash_layout.h"
#include <string.h>

#define COPY_BUFFER_SIZE 256

/* HELPERS--------------------------------------- */

/**
 * @brief Copy a firmware image from one flash region to another.
 *
 * This helper performs chunked copying of a firmware image from a source
 * flash address to a destination flash address.
 *
 * - Copies firmware in fixed-size chunks
 * - Pads incomplete chunks with `0xFF`
 * - Writes aligned flash words
 * - Tracks progress through logging
 *
 * The application header size is automatically included in the copied size.
 *
 * @param copy_start_addr Start address of the source firmware image.
 * @param write_start_addr Destination flash address where firmware
 *                         will be written.
 * @param copy_app_size Size of the firmware payload excluding the
 *                      application header.
 *
 * @retval true  Firmware copied successfully.
 * @retval false Flash write failed during copy operation.
 *
 * @note Destination flash sectors must be erased before calling this
 *       function.
 * @note Flash writes are aligned to 32-bit words.
 */
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

  custom_logger_log("[BOOTy]: Entering fw_copy_to_address\r\n");

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
      custom_logger_log("[BOOTy]: Error in Flash_Write_Data with ret: %d\r\n",
                        ret);
      return false;
    }
    /* we wrote full words, nothing is leftover */
    flash_write_addr += words * 4;
    bytes_copied += chunk;

    custom_logger_log("[BOOTy]: Bytes copied: {%d} / {%d}\r", bytes_copied,
                      fw_app_size);
  }

  custom_logger_log("[BOOTy]: fw_copy_to_sector succeeded in copying app\r\n");
  return true;
}

/* funtions --------------------------------------- */

/**
 * @brief Check whether a firmware update is available.
 *
 * Compares the firmware version stored in the update slot with the
 * currently running application version.
 *
 * If the update version is newer, the bootloader will proceed with
 * the update process.
 *
 * Older firmware images are still kept as fallback firmware in case
 * the main application becomes corrupted.
 *
 * @retval true  A newer firmware update is available.
 * @retval false No newer update is available.
 *
 * @note The update mechanism intentionally allows retry behavior in
 *       case the update process is interrupted during flashing.
 */
bool bl_check_for_update(void) {
  custom_logger_log("[BOOTy]: Current app version is: %x\r\n",
                    get_app_header()->version);

  custom_logger_log("[BOOTy]: Current update_app_header version is: %x\r\n",
                    get_update_header()->version);

  bool cmp_res = (get_update_header()->version > get_app_header()->version);

  if (cmp_res) {
    custom_logger_log(
        "[BOOTy]: Update version newer, will proceed with update..\r\n");
  } else {
    custom_logger_log(
        "[BOOTy]: Update version is older. Need to manually flash it or it "
        "will be used as a backup in case main application is corrupted.\r\n");
  }

  return cmp_res;
}

/**
 * @brief Apply a firmware update from the update storage slot.
 *
 * This function performs the complete update procedure:
 * 1. Verify the update firmware integrity
 * 2. Erase the main application flash sectors
 * 3. Copy the update firmware into the main application region
 *
 * @retval true  Firmware update applied successfully.
 * @retval false Verification or flash copy operation failed.
 *
 * @warning If power loss or interruption occurs during flashing,
 *          the main application may become temporarily invalid until
 *          the update process is retried.
 *
 * @note The update image is not erased automatically after update.
 */
bool bl_apply_update(void) {
  /* verify if the app from update is valid */
  enum verify_result_t res = bl_verify_app(get_update_header());
  if (res != VERIFY_OK) {
    custom_logger_log(
        "[BOOTy]: Failed verifying updated firmware app with reason: %d\r\n",
        res);
    return false;
  }

  /* erase sector 2 to sector 6, this is main app */
  Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
  /* i need some way to move chunks from update slot to main app slot */
  bool ret = fw_copy_to_address(UPDATE_STORAGE_START_ADDR, APP_HEADER_ADDR,
                                get_update_header()->size);
  if (!ret) {
    custom_logger_log("[BOOTy]: fw_copy_to_address failed in copying\r\n");
  }
  return true;
  //
}

/**
 * @brief Erase the firmware update storage sector.
 *
 * Removes the stored update firmware image from flash memory.
 *
 * @retval true  Sector erase failed.
 * @retval false Sector erased successfully.
 *
 * @warning Return semantics are inverted due to direct comparison with
 *          HAL flash error codes.
 */
bool bl_clear_update_sector(void) {
  return Flash_Erase_Sectors(FLASH_SECTOR_6, 1) != HAL_FLASH_ERROR_NONE;
}

/* Removed because im not using RAM swap  */
// bool bl_swap_partitions(void) {

//   /* check if the update fw is valid */
//   enum verify_result_t res = bl_verify_app(get_update_header());
//   if (res != VERIFY_OK) {
//     custom_logger_log(
//         "[BOOTy]: Failed verifying updated firmware app with reason: %d\n",
//         res);
//     return false;
//   }

//   /* i need to grab the fw into ram, overwrite it with the main app, move
//   update
//    * from ram to main slot */
//   const uint32_t fw_update_size = get_update_header()->size + 512;
//   uint8_t fw_update_buff[fw_update_size];
//   memcpy(fw_update_buff, get_update_header(), fw_update_size);

//   /* remove update fw sectors */
//   Flash_Erase_Sectors(FLASH_SECTOR_6, 1);
//   /* move the main app to the update sector */
//   bool ret = fw_copy_to_address(APP_HEADER_ADDR, UPDATE_STORAGE_START_ADDR,
//                                 get_app_header()->size);
//   if (!ret) {
//     custom_logger_log("Error copying main app to new address, exiting\r\n");
//     return false;
//   }

//   /* move update to main app's section */
//   Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
//   ret = fw_copy_to_address((uint32_t)fw_update_buff, APP_HEADER_ADDR,
//                            fw_update_size);

//   if (!ret) {
//     custom_logger_log(
//         "Error copying copied firmware from ram to new address,
//         exiting\r\n");
//     return false;
//   }

//   /* TODO: this will brick the device if some error happens in between lol,
//   very
//    * unsafe */
//   return true;
// }