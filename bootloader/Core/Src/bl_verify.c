#include "bl_verify.h"
#include "custom_crc32.h"
#include "main.h"
#include "tinycrypt/ecc_dsa.h"
#include "tinycrypt/sha256.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "custom_logger.h"
#include "flash/operations.h"

extern UART_HandleTypeDef huart1;

/* TODO: For later usage. This will be version 1.0, first 4 bits are major version, second 4 are minor version. Will see how can i implement anti rollback for this one */
#define MIN_VERSION 0x00010000U

// clang-format off
static const uint8_t _PUBLIC_KEY[64] = {
    //   Public key X:
    //   7224de17d13ee1eee3953d976f7bff317616bb8dc1dc7c8b15f438f8bd26bab0
    // Public key Y:
    // 22e43107ec33dfa5c14b8035bf354e823b59729bb6638f0c9f5a6eea640efe78
    /* X */
    0x72, 0x24, 0xde, 0x17, 0xd1, 0x3e, 0xe1, 0xee,
    0xe3, 0x95, 0x3d, 0x97, 0x6f, 0x7b, 0xff, 0x31,
    0x76, 0x16, 0xbb, 0x8d, 0xc1, 0xdc, 0x7c, 0x8b,
    0x15, 0xf4, 0x38, 0xf8, 0xbd, 0x26, 0xba, 0xb0,
    /* Y */
    0x22, 0xe4, 0x31, 0x07, 0xec, 0x33, 0xdf, 0xa5,
    0xc1, 0x4b, 0x80, 0x35, 0xbf, 0x35, 0x4e, 0x82,
    0x3b, 0x59, 0x72, 0x9b, 0xb6, 0x63, 0x8f, 0x0c,
    0x9f, 0x5a, 0x6e, 0xea, 0x64, 0x0e, 0xfe, 0x78,
};
// clang-format on
// /*
// enum verify_result_t bl_verify_update() {
//   const struct app_header_t *app_header = (const struct app_header_t
//   *)address; if (app_header->update == 0) {
//     custom_logger_log("[BL]: update flag is: 0");
//     return VERIFY_NO_UPDATE_AVAILABLE;
//   }
//   return VERIFY_UPDATE_AVAILABLE;
// }

// enum verify_result_t bl_update_fw(void) {
//   enum verify_result_t res = bl_verify_app(UPDATE_STORAGE_START_ADDR);
//   if (res != VERIFY_OK) {
//     custom_logger_log("[BL]: Problem with verifying updated app fw");
//     return res;
//   }

//   // update available
//   /* i need to check if the update is legit, so ill have to do the same thing
//    * bl_verify_app but for the starting address of the update sector; after
//    that i need to move the new app to sector 2-5. once moved, remove update
//    from flash  and then the code will check it again one problem: i have
//    update bit flag set, but it doesnt matter since i will be removing it from
//    flash s6-7 why do i need this flag at all then? if i have something in the
//    flash s6 and 7, i should just check that and write it to the main app
//    sectors?
//    */

//   // remove main app sectors
//   bl_remove_sectors(FLASH_SECTOR_2, 4);
//   // start moving the update to the app sectors
//   // TODO: Write a function for this
//   uint8_t fw_buf[256];
//   uint32_t fw_buf_idx = 0;
//   uint32_t flash_write_addr = APP_HEADER_ADDR;

//   const uin32_t FLASH_WRITE_CHUNK = 256;
//   while ((flash_write_addr - APP_HEADER_ADDR) < 12000) {
//     fw_buf[fw_buf_idx++] = byte;

//     if (fw_buf_idx == FLASH_WRITE_CHUNK) {
//       // buffer full,  to flash
//       Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf,
//                        FLASH_WRITE_CHUNK / 4);
//       flash_write_addr += FLASH_WRITE_CHUNK;
//       fw_buf_idx = 0;
//     }
//   }
//   return VERIFY_OK;
// } */

// int32_t bl_remove_sectors(uint32_t sector, uint32_t num_sectors) {
//   int32_t res = Flash_Erase_Sectors(sector, num_sectors);
// }

enum verify_result_t bl_verify_app(const struct app_header_t *app_header) {
  custom_logger_log("[BL]: bl_veify_app address: %x\r\n", app_header);
  custom_logger_log("[BL]: bl_verify_app: verify magic constant\r\n");
  /* Checking magic constant */
  if (app_header->magic != APP_MAGIC_CONSTANT) {
    return VERIFY_BAD_MAGIC;
  }

  custom_logger_log("[BL]: bl_verify_app: verify fw size\r\n");
  /* Check firmware size */
  if (app_header->size == 0 || app_header->size > APP_MAX_SIZE) {
    return VERIFY_BAD_SIZE;
  }

  /* CRC check
    check if the fw code is the one we expect
    we are doing our own crc calculation and comparing it to app header's crc
  */

  custom_logger_log("[BL]: bl_verify_app: verify CRC check\r\n");
  /* TODO: Fix magic constant, 512 is because thats the padding for the header
   */

  custom_logger_log("[BL]: im checking address %x\r\n",
                    (const uint8_t *)app_header + 512);
  uint32_t crc = crc32((const uint8_t *)app_header + 512, app_header->size);
  if (crc != app_header->crc) {
    return VERIFY_BAD_CRC;
  }
  /* SHA 256 */
  uint8_t digest[32];
  struct tc_sha256_state_struct s;
  (void)tc_sha256_init(&s);
  /* TODO: Fix magic constant, 512 is because thats the padding for the header
   */
  tc_sha256_update(&s, (const uint8_t *)app_header + 512, app_header->size);
  (void)tc_sha256_final(digest, &s);

  char msg[100];
  snprintf(msg, 100, "BOOTy: before ecdsa\r\n");
  custom_logger_log(msg);

  /* Signature */
  int ecdsa_res = uECC_verify(_PUBLIC_KEY, digest, sizeof(digest),
                              app_header->signature, uECC_secp256r1());

  custom_logger_log("[BL]: bl_verify_app: verify signature\r\n");
  if (ecdsa_res != 1) {
    return VERIFY_BAD_SIGNATURE;
  }
  custom_logger_log("[BL]: bl_verify_app: all ok\r\n");

  /* TODO:VERIFY_BAD_VERSION For later, add anti rollback guard */

  return VERIFY_OK;
}

bool bl_check_for_update(void) {

  const struct app_header_t *app_header =
      (const struct app_header_t *)APP_HEADER_ADDR;

  const struct app_header_t *update_app_header =
      (const struct app_header_t *)UPDATE_STORAGE_START_ADDR;

  custom_logger_log("Current app version is: %x", app_header->version);

  custom_logger_log("Current update_app_header version is: %x",
                    update_app_header->version);

  return (update_app_header->version > app_header->version);
}

bool bl_swap_updates(void) {

  const struct app_header_t *app_header =
      (const struct app_header_t *)UPDATE_STORAGE_START_ADDR;
  /* verify if the app from update is valid */
  enum verify_result_t res = bl_verify_app(app_header);
  if (res != VERIFY_OK) {
    custom_logger_log(
        "[BL]: Failed verifying updated firmware app with reason: %d\n", res);
    return false;
  }

  /* erase sector 2 to sector 6, this is main app */
  Flash_Erase_Sectors(FLASH_SECTOR_2, 4);
  /* i need some way to move chunks from update slot to main app slot */
  uint8_t *src = (uint8_t *)UPDATE_STORAGE_START_ADDR;
  uint8_t fw_buf[256];
  uint32_t bytes_copied = 0;
  uint32_t fw_size = app_header->size + 512;
  uint32_t flash_write_addr = APP_HEADER_ADDR;
  const uint32_t CHUNK = 256;
  int32_t chunk;
  while (bytes_copied < fw_size) {
    chunk = (fw_size - bytes_copied) < CHUNK ? (fw_size - bytes_copied) : CHUNK;
    memcpy(fw_buf, src + bytes_copied, chunk);
    Flash_Write_Data(flash_write_addr, (uint32_t *)fw_buf, chunk / 4);
    flash_write_addr += chunk;
    bytes_copied += chunk;
  }
  return true;
  //
}
