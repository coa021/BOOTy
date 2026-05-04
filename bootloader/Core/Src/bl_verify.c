#include "bl_verify.h"


/* TODO: For later usage. This will be version 1.0, first 4 bits are major version, second 4 are minor version. Will see how can i implement anti rollback for this one */
#define MIN_VERSION 0x00010000U


enum verify_result_t bl_verify_app(void)
{
  const struct app_header_t *app_header =
      (const struct app_header_t *)APP_HEADER_ADDR;

  /* Checking magic constant */
  if (app_header->magic != APP_MAGIC_CONSTANT) {
    return VERIFY_BAD_MAGIC;
  }

  /* Check firmware size */
  if (app_header->size == 0 || app_header->size > APP_MAX_SIZE) {
    return VERIFY_BAD_SIZE;
  }

  /* CRC check
    check if the fw code is the one we expect
    we are doing our own crc calculation and comparing it to app header's crc
  */

  /* TODO: For later add anti rollback guard */
  /* TODO: add sha256 and signature checks */

  return VERIFY_OK;
}