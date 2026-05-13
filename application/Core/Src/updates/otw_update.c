#include "updates/otw_update.h"
#include "main.h"

#include <string.h>

#include "custom_crc/custom_crc.h"
#include "flash/operations.h"
#include "flash_layout.h"

#define OTW_PACKET_HEADER_LEN 3
#define FLASH_WRITE_CHUNK 256

/*  helpers */
/* sending ack */

/* ack */
static void otw_send_ack(struct otw_update_t *update) {
  static uint8_t byte = ACK;
  HAL_UART_Transmit(update->huart, &byte, 1, 0xFF);
}

/* nack */
static void otw_send_nack(struct otw_update_t *update) {
  static uint8_t byte = NACK;
  HAL_UART_Transmit(update->huart, &byte, 1, 0xFF);
}

/* check crc */
static bool otw_update_verify_crc(const struct otw_uart_packet_t *p) {
  uint8_t buf[OTW_PACKET_HEADER_LEN + MAX_PAYLOAD_SIZE];

  buf[0] = p->cmd;
  buf[1] = (uint8_t)(p->length & 0xFF); // low
  buf[2] = (uint8_t)(p->length >> 8);   // high
  memcpy(&buf[3], p->payload, p->length);

  uint32_t computed = crc32(buf, OTW_PACKET_HEADER_LEN + p->length);
  return computed == p->crc;
}

void otw_update_init(struct otw_update_t *update, UART_HandleTypeDef *huart) {
  //
  *update = (struct otw_update_t){
      .huart = huart, .expected_fw_size = 0, .bytes_written = 0};
}

bool otw_update_handle_packet(struct otw_update_t *update,
                              const struct otw_uart_packet_t *packet) {
  //

  if (!otw_update_verify_crc(packet)) {
    otw_send_nack(update);
    return false;
  }

  switch (packet->cmd) {
  case OTW_CMD_START: {

    if (packet->length < 4) {
      otw_send_nack(update);
      return false;
    }
    update->expected_fw_size = ((uint32_t)packet->payload[0]) |
                               ((uint32_t)packet->payload[1] << 8) |
                               ((uint32_t)packet->payload[2] << 16) |
                               ((uint32_t)packet->payload[3] << 24);

    update->bytes_written = 0;

    // erase target sector
    // it would be 2 sectors, 6 and 7 of update partitoin. but realistically im
    // not sure ill be sending more than 128KB, so i could split this a bit
    // better and save all the unnecessary FLASH erase's
    Flash_Erase_Sectors(FLASH_SECTOR_6, 2);

    otw_send_ack(update);
    break;
  }
  case OTW_CMD_DATA: {

    if (packet->cmd != OTW_CMD_DATA) {
      otw_send_nack(update);
      return false;
    }
    // write data to sector
    uint32_t temp[(MAX_PAYLOAD_SIZE + 3) / 4];
    uint32_t words = (packet->length) / 4;
    memset(temp, 0xFF, words * 4);
    memcpy(temp, packet->payload, packet->length);

    Flash_Write_Data(UPDATE_STORAGE_START_ADDR + update->bytes_written, temp,
                     words);

    update->bytes_written += words * 4;

    otw_send_ack(update);
    return false;
  }
  case OTW_CMD_END: {
    /* data is not transmitted till end */
    if (update->expected_fw_size > 0 &&
        update->bytes_written != update->expected_fw_size) {
      otw_send_nack(update);
      return false;
    }

    /* data is completely transmitted */
    otw_send_ack(update);
    return true;
  }
  default: {
    // unknown
    otw_send_nack(update);
    return false;
  }
  }

  /* just in case */
  return false;
}

void otw_update_handle_error(struct otw_update_t *update) {
  //
  otw_send_nack(update);
}
