#ifndef OTW_UPDATE_H_
#define OTW_UPDATE_H_

#define OTW_FLAG_START 1

#include <stdint.h>

void otw_update_receive_byte(uint8_t byte);
void otw_update_init(void);
void otw_update_flush(void);

#endif // OTW_UPDATE_H_
