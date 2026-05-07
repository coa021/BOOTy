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
                    (const uint8_t *)app_header + APP_HEADER_SIZE);
  uint32_t crc = crc32((const uint8_t *)app_header + APP_HEADER_SIZE, app_header->size);
  if (crc != app_header->crc) {
    return VERIFY_BAD_CRC;
  }
  /* SHA 256 */
  uint8_t digest[32];
  struct tc_sha256_state_struct s;
  (void)tc_sha256_init(&s);
  /* TODO: Fix magic constant, 512 is because thats the padding for the header
   */
  tc_sha256_update(&s, (const uint8_t *)app_header + APP_HEADER_SIZE, app_header->size);
  (void)tc_sha256_final(digest, &s);

  custom_logger_log("[BL]: before ecdsa\r\n");

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
