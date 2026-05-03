#ifndef INC_BL_VERIFY_H_
#define INC_BL_VERIFY_H_

#include <stdint.h>

#include "app_header.h"

enum verify_result_t {
    VERIFY_OK,
    VERIFY_BAD_MAGIC,
    VERIFY_BAD_SIZE,
    VERIFY_BAD_VERSION,
    VERIFY_BAD_HASH,
    VERIFY_BAD_SIGNATURE,
};

enum verify_result_t bl_verify_app(void);

#endif