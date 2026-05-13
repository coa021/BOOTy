#include "uart_update/update_packet_validator.h"

#include "custom_crc/custom_crc.h"
#include "custom_logger.h"
#include "uart_update/update_packet_parser.h"

#include "app_header.h"

/**
 * @brief Validate received chunk size
 *
 * Checks if the packet is not malformed, or if we have enough memory to store
 * this new chunk. write_idx represents how much bytes have we wrote so far
 *
 * @param received Address for where to write the received chunk
 * @param write_idx Current write index
 * @param max_size Max size of out application/new firmware
 * @return true/false to signal if we wrote chunk successfully or not
 */
bool update_packet_validate_size(const uint16_t received,
                                 const uint32_t write_idx,
                                 const uint32_t max_size) {
  //
  if (received < UPDATE_PACKET_MIN_SIZE ||
      received > UPDATE_PACKET_BUFFER_SIZE) {
    custom_logger_log("Error: packet size problem, size is {%d}\r\n", received);

    return false;
  }

  if (write_idx > max_size) {
    custom_logger_log("Error: write idx problem, write_idx is {%d}. MAX app "
                      "size is: {%d}\r\n",
                      write_idx, max_size);

    return false;
  }
  return true;
}

/**
 * @brief Check crc16 of received chunk
 *
 *
 * @param buffer Pointer to the buffer that stores chunk data/payload
 * @param received Received payload size
 * @return true/false if crc16 matches expected
 */
bool update_packet_validate_crc16(const uint8_t *buffer, uint16_t received) {
  //

  if (received < UPDATE_PACKET_OVERHEAD_SIZE) {
    custom_logger_log(
        "Error: Malformed package format. Size of package is: {%d}\r\n",
        received);

    return false;
  }

  /* i need to extract last 2 bytes of the package */
  /* TODO: sanity check, my overhead is now 7 bytes */
  /* since crc16 is always last 2 bytes, i subtract that */
  uint16_t payload_size = received - UPDATE_PACKET_OVERHEAD_SIZE;
  uint16_t calculated_crc16 =
      crc16(buffer + UPDATE_PACKET_HEADER_SIZE, payload_size);

  uint16_t crc_offset = received - UPDATE_PACKET_CRC16_SIZE;
  uint16_t expected_crc16 =
      (uint16_t)buffer[crc_offset] | (uint16_t)(buffer[crc_offset + 1] << 8);

  if (calculated_crc16 != expected_crc16) {
    custom_logger_log("Missmatch in crc16; expected: %d,\tactual: %d\r\n",
                      expected_crc16, calculated_crc16);

    return false;
  }
  return true;
}

/**
 * @brief Write chunk to FLASH
 *
 * Validate app header, and if it is valid assign the firmware size to the
 * out_size parameter
 *
 * @param buffer pointer to uint8_t buffer that stored app header
 * @param out_size Pointer to where i want to write the size of firmware
 * @return true/false if the header is valid
 */
bool update_packet_validate_app_header(const uint8_t *buffer,
                                       uint32_t *out_size) {
  //
  struct app_header_t *hdr =
      (struct app_header_t *)(buffer + UPDATE_PACKET_HEADER_SIZE);

  if (hdr->magic != APP_MAGIC_CONSTANT) {
    custom_logger_log("Error with app header in new package. Missing MAGIC "
                      "constant, couldn't verify integrity of header\r\n");

    return false;
  }

  /* validate if the app can fit here, but what if the user changed the size in
   * the header, i will have a problem then */
  uint32_t total_size = hdr->size + APP_HEADER_SIZE;
  if (total_size > APP_MAX_SIZE) {
    custom_logger_log("Error. Firmware cannot fit on the FLASH update sector. "
                      "MAX Size is %d (~%dKB)!\r\n",
                      APP_MAX_SIZE, (APP_MAX_SIZE / 1024));
    return false;
  }

  *out_size = total_size;

  return true;
}
