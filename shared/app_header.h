#ifndef _SHARED_APP_HEADER_H
#define _SHARED_APP_HEADER_H

#include "flash_layout.h"

#include <stdint.h>

#define APP_MAGIC_CONSTANT 0x0B00B1E5U

struct app_header_t {
  uint32_t magic;
  uint32_t version;
  uint32_t size;
  uint32_t crc;
  uint8_t sha256[32];
  uint8_t signature[64];
};


#endif 