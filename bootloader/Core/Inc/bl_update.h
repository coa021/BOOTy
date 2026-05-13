#ifndef BL_UPDATE_H_
#define BL_UPDATE_H_

#include <stdbool.h>

// Code for header body

bool bl_check_for_update(void);
bool bl_apply_update(void);

bool bl_clear_update_sector(void);
/* bool bl_swap_partitions(void); */

#endif // BL_UPDATE_H_