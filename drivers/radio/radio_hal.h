#ifndef __RADIO_HAL_H__
#define __RADIO_HAL_H__

#include <emodules/mod_defs.h>
#include <emodules/mod_mcu.h>
#include <emodules/mod_fm.h>
#include <kconfig.h>
#include "dfs_posix.h"



// --- HAL 返回码 ---
typedef enum {
    RADIO_HAL_OK = 0,
    RADIO_HAL_ERROR = -1,
    RADIO_HAL_DEVICE_NOT_OPEN = -2,
} radio_hal_status_t;

//Tuner Local Seek define
#define LOCAL_SEEK_ON	1
#define LOCAL_SEEK_OFF  0
//Tuner RDS On Off define
#define RDS_ON			1
#define RDS_OFF			0
//Tuner RDS AF On Off define
#define RDS_AF_ON		1
#define RDS_AF_OFF		0
//Tuner RDS TA On Off define
#define RDS_TA_ON		1
#define RDS_TA_OFF		0
//Tuner RDS CT On Off define
#define RDS_CT_ON		1
#define RDS_CT_OFF		0


// --- 1. 初始化与反初始化 ---
radio_hal_status_t radio_hal_init(void);
void radio_hal_deinit(void);
bool radio_hal_is_ready(void);


// --- 2. 核心调谐与播放 ---
radio_hal_status_t radio_hal_set_frequency(uint32_t freq_khz);
radio_hal_status_t radio_hal_play_preset(uint32_t preset_index); // preset_index 从 1 开始


// --- 3. 搜台控制 ---
radio_hal_status_t radio_hal_seek_up(void);
radio_hal_status_t radio_hal_seek_down(void);
radio_hal_status_t radio_hal_tune_up(void);   // 手动微调
radio_hal_status_t radio_hal_tune_down(void);
radio_hal_status_t radio_hal_seek_stop(void);
radio_hal_status_t radio_hal_auto_store(void); // 自动存台
radio_hal_status_t radio_hal_pty_seek(uint32_t pty_code);


// --- 4. 配置与设置 ---
radio_hal_status_t radio_hal_switch_band(uint32_t band_index); // 0-2 for FM1-3, 3-4 for AM1-2
radio_hal_status_t radio_hal_switch_band_and_tune(uint32_t band_index,uint32_t preset_index); // 切换波段并调谐到指定预设
radio_hal_status_t radio_hal_set_preset(uint32_t preset_index); // preset_index 从 1 开始
radio_hal_status_t radio_hal_cancel_preset(uint32_t preset_index);
radio_hal_status_t radio_hal_set_area(uint32_t area_code);
radio_hal_status_t radio_hal_set_loc_dx_mode(bool is_local);


// --- 5. RDS 功能控制 ---
radio_hal_status_t radio_hal_set_rds_enabled(bool enabled);
radio_hal_status_t radio_hal_set_rds_af_enabled(bool enabled);
radio_hal_status_t radio_hal_set_rds_ta_enabled(bool enabled);
radio_hal_status_t radio_hal_set_rds_ct_enabled(bool enabled);
radio_hal_status_t radio_hal_set_seek_type(uint32_t seek_type);

// --- 6. 状态查询
__s32 radio_hal_fm_get_init_status(void);



#endif