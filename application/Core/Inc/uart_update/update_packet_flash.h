#ifndef UPDATE_PACKET_FLASH_H_
#define UPDATE_PACKET_FLASH_H_

#include <stdbool.h>
#include <stdint.h>

#define UPDATE_PACKET_FLASH_SECTOR_START FLASH_SECTOR_6
#define UPDATE_PACKET_FLASH_SECTOR_NUM_REMOVE 2

bool update_packet_flash_erase_update(void);
bool update_packet_flash_write_chunk(uint32_t dest_addr,
                                     const uint8_t *payload,
                                     uint16_t payload_size);
bool update_packet_flash_validate_image_crc32(uint32_t start_addr,
                                              uint32_t fw_size);
#endif // UPDATE_PACKET_FLASH_H_