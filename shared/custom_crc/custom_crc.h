/** @file      custom_crc.h
 * @brief      Header for the commonly shared crc32 and crc16 functions
 */

#ifndef SHARED_CUSTOM_CRC_H_
#define SHARED_CUSTOM_CRC_H_

#include <stdint.h>
#include <stddef.h>

uint32_t crc32(const uint8_t *data, uint32_t length);

uint16_t crc16(const uint8_t *data, size_t length);

#endif /* INC_CRC32_H_ */
