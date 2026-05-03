#ifndef _APP_HEADER_H
#define _APP_HEADER_H

struct app_header_t {
    uint32_t magic;
    uint32_t size;
    uint32_t crc;
    uint32_t version;
};


#endif 