#include "uart_update/update_packet_validator.h"

#include "custom_crc/custom_crc32.h"
#include "custom_logger.h"
#include "uart_update/update_packet_parser.h"

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

bool update_packet_validate_crc16(const uint8_t *buffer, uint16_t received) {
  //

  if (received < UPDATE_PACKET_OVERHEAD_SIZE) {
    custom_logger_log(
        "Error: Malformed package format. Size of package is: {%d}\r\n",
        received);

    return false;
  }

  /* i need to extract last 2 bytes of the package */
  uint16_t payload_size = received - UPDATE_PACKET_OVERHEAD_SIZE;
  uint16_t calculated_crc16 = crc16(buffer, payload_size);
  uint16_t expected_crc16 =
      buffer[payload_size] | (buffer[payload_size + 1] << 8);

  if (calculated_crc16 != expected_crc16) {
    custom_logger_log("Missmatch in crc16; expected: %d,\tactual: %d\r\n",
                      expected_crc16, calculated_crc16);

    return false;
  }
  return true;
}
