/**
 * @file bl_verify.c
 * @brief Firmware image verification implementation for the bootloader.
 *
 * This module validates firmware images before execution or installation.
 *
 * Verification stages include:
 * - Application magic constant validation
 * - Firmware size validation
 * - CRC32 integrity verification
 * - SHA-256 digest generation
 * - ECDSA signature verification
 * - (Planned) anti-rollback version validation
 *
 * The verification process ensures that only valid and cryptographically
 * signed firmware images are accepted by the bootloader.
 */

#include "bl_verify.h"
#include "custom_crc/custom_crc.h"
#include "main.h"
#include "tinycrypt/ecc_dsa.h"
#include "tinycrypt/sha256.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "custom_logger.h"
#include "flash/operations.h"

// extern UART_HandleTypeDef huart1;

/* TODO: For later usage. This will be version 1.0, first 4 bits are major
 * version, second 4 are minor version. Will see how can i implement anti
 * rollback for this one */
#define MIN_VERSION 0x00010000U

/**
 * @brief ECDSA public key used for firmware signature verification.
 *
 * This public key is used to validate firmware signatures generated
 * using the corresponding private key.
 *
 */
// clang-format off
static const uint8_t _PUBLIC_KEY[64] = {
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

/**
 * @brief Verify firmware image integrity and authenticity.
 *
 * Performs a full validation sequence on the provided firmware image:
 *
 * 1. Validate application magic constant
 * 2. Validate firmware size boundaries
 * 3. Verify firmware CRC32 checksum
 * 4. Generate SHA-256 digest of firmware payload
 * 5. Verify ECDSA signature using embedded public key
 *
 * The firmware payload starts immediately after the application header.
 *
 * @param app_header Pointer to the firmware application header.
 *
 * @retval VERIFY_OK             Firmware is valid and trusted.
 * @retval VERIFY_BAD_MAGIC      Invalid application magic constant.
 * @retval VERIFY_BAD_SIZE       Invalid firmware size.
 * @retval VERIFY_BAD_CRC        CRC32 integrity verification failed.
 * @retval VERIFY_BAD_SIGNATURE  ECDSA signature verification failed.
 *
 */
enum verify_result_t bl_verify_app(const struct app_header_t *app_header) {
  custom_logger_log("[BOOTy]: bl_veify_app address: %x\r\n", app_header);
  custom_logger_log("[BOOTy]: bl_verify_app: verify magic constant\r\n");
  /* Checking magic constant */
  if (app_header->magic != APP_MAGIC_CONSTANT) {
    return VERIFY_BAD_MAGIC;
  }

  custom_logger_log("[BOOTy]: bl_verify_app: verify fw size\r\n");
  /* Check firmware size */
  if (app_header->size == 0 || app_header->size > APP_MAX_SIZE) {
    return VERIFY_BAD_SIZE;
  }

  /* CRC check
    check if the fw code is the one we expect
    we are doing our own crc calculation and comparing it to app header's crc
  */

  /*
  TODO: Critical error! I have sent half a package and bricked my device XD im
  updating only when update version is GT main app version */

  custom_logger_log("[BOOTy]: bl_verify_app: verify CRC check\r\n");

  custom_logger_log("[BOOTy]: im checking address %x\r\n",
                    (const uint8_t *)app_header + APP_HEADER_SIZE);
  uint32_t crc =
      crc32((const uint8_t *)app_header + APP_HEADER_SIZE, app_header->size);
  if (crc != app_header->crc) {
    return VERIFY_BAD_CRC;
  }
  /* SHA 256 */
  uint8_t digest[32];
  struct tc_sha256_state_struct s;
  (void)tc_sha256_init(&s);
  tc_sha256_update(&s, (const uint8_t *)app_header + APP_HEADER_SIZE,
                   app_header->size);
  (void)tc_sha256_final(digest, &s);

  custom_logger_log("[BOOTy]: before ecdsa\r\n");

  /* Signature */
  int ecdsa_res = uECC_verify(_PUBLIC_KEY, digest, sizeof(digest),
                              app_header->signature, uECC_secp256r1());

  custom_logger_log("[BOOTy]: bl_verify_app: verify signature\r\n");
  if (ecdsa_res != 1) {
    return VERIFY_BAD_SIGNATURE;
  }

  custom_logger_log("[BOOTy]: bl_verify_app: all ok\r\n");

  /* TODO:VERIFY_BAD_VERSION For later, add anti rollback guard */

  return VERIFY_OK;
}
