#ifndef __MCU_HAL_H__
#define __MCU_HAL_H__
    
#include "mod_mcu.h"
#include <log.h>
#include "dfs_posix.h"

__s32 mcu_hal_init_para(void);
void *mcu_hal_get_reg_handle_by_service_id(__u32 eApp);

#endif