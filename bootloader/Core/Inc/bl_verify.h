#ifndef INC_BL_VERIFY_H_
#define INC_BL_VERIFY_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_header.h"

enum verify_result_t {
  VERIFY_OK,
  VERIFY_BAD_MAGIC,
  VERIFY_BAD_SIZE,
  VERIFY_BAD_VERSION,
  VERIFY_BAD_CRC,
  VERIFY_BAD_SIGNATURE,
  // for updates
  VERIFY_UPDATE_AVAILABLE,
  VERIFY_NO_UPDATE_AVAILABLE,
};

// enum verify_result_t bl_verify_update(void);
enum verify_result_t bl_verify_app(const struct app_header_t *app_header);

bool bl_check_for_update(void);

bool bl_swap_updates(void);
// enum verify_result_t bl_update_fw(void) int32_t
//     bl_remove_sectors(uint32_t sector, uint32_t num_sectors);
#endif
