#ifndef UPDATE_PACKET_VALIDATOR_H_
#define UPDATE_PACKET_VALIDATOR_H_

#include <stdbool.h>
#include <stdint.h>

bool update_packet_validate_size(const uint16_t received,
                                 const uint32_t write_idx,
                                 const uint32_t max_size);
bool update_packet_validate_crc16(const uint8_t *buffer, uint16_t received);

#endif // UPDATE_PACKET_VALIDATOR_H_
