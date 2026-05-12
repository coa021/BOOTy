#include "custom_crc32.h"

uint32_t crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}

uint16_t crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF; // Initial value
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];          // XOR byte into least sig. byte of crc
        for (int j = 8; j != 0; j--) {     // Loop over each bit
            if ((crc & 0x0001) != 0) {     // If the LSB is set
                crc >>= 1;                 // Shift right and XOR polynomial
                crc ^= 0xA001;
            } else {                       // Else just shift right
                crc >>= 1;
            }
        }
    }
    return crc;
}
