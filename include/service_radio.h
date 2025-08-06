#ifndef __SERVICE_RADIO_H__
#define __SERVICE_RADIO_H__   
#include <stdint.h>
#include <infotainment_os_api.h>
#include <bus_router.h>
#include <radio_api.h>


// 【新增】定义 Radio 服务的内部状态
typedef enum {
    RADIO_STATE_IDLE,           // 空闲状态 (例如，未播放，等待指令)
    RADIO_STATE_INIT,           // 初始化状态
    RADIO_STATE_PLAYING,        // 正常播放状态
    RADIO_STATE_SEEKING,        // 正在自动搜台 (SEEK)
    RADIO_STATE_TUNING,         // 正在手动微调 (TUNE)
    RADIO_STATE_AUTO_STORING,   // 正在自动存台 (AS)
    RADIO_STATE_PTY_SEEK,   // 
    RADIO_STATE_PLAY_PRESET,   // 
    RADIO_STATE_SAVE_PRESET,   // 
    RADIO_STATE_SWITCH_BAND,   // 
    RADIO_STATE_SWITCH_BAND_AND_TUNE,   // 
    RADIO_STATE_RDS,   // 
} radio_state_t;



// 【新增】定义 Radio 服务内部使用的定时器类型
typedef enum {
    RADIO_TIMER_NONE = 0,
    RADIO_TIMER_CHECK_HARDWARE_STATUS,      // 硬件状态检查定时器
    RADIO_TIMER_SEEK_UPDATE_UI,       // 搜台更新UI定时器
    RADIO_TIMER_SEEK_TIMEOUT,       // 搜台超时定时器
    RADIO_TIMER_TUNE_STATUS,          // 长按微调的状态定时器
    RADIO_TIMER_TUNE_STEP,          // 长按微调的步进定时器
    RADIO_TIMER_RDS_SCROLL,         // RDS 文本滚动定时器
    RADIO_TIMER_SHOW_DIALOG,        // 临时对话框消失定时器
    RADIO_TIMER_PLAYING_UPDATE_UI,        // 播放状态更新UI定时器
    RADIO_TIMER_PRESET_SAVE_STATUS,        // 
    RADIO_TIMER_PRESET_PLAY_STATUS,        // 
    RADIO_TIMER_SWITCH_BAND_STATUS,        // 
    RADIO_TIMER_SWITCH_BAND_AND_TUNE_STATUS,        // 
    RADIO_TIMER_CHECK_RDS_STATUS,        // 

    RADIO_TIMER_TYPE_COUNT          // 内部定时器的总数
} radio_internal_timer_type_t;

typedef enum { // 内部命令
    RADIO_CMD_INTERNAL_TIMER_CHECK_HARDWARE_STATUS = SRC_RADIO_SERVICE+0xF000,      // 硬件状态检查定时器
    RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT,          // 搜台超时定时器
    RADIO_CMD_INTERNAL_TIMER_TUNE_STATUS,             // 长按微调的状态定时器
    RADIO_CMD_INTERNAL_TIMER_TUNE_STEP,             // 长按微调的步进定时器
    RADIO_CMD_INTERNAL_TIMER_PLAYING_UPDATE_UI,             // 播放状态更新UI定时器
    RADIO_CMD_INTERNAL_TIMER_PRESET_SAVE_STATUS,             // 
    RADIO_CMD_INTERNAL_TIMER_PRESET_PLAY_STATUS,             // 
    RADIO_CMD_INTERNAL_TIMER_SWITCH_BAND_STATUS,             // 
    RADIO_CMD_INTERNAL_TIMER_SWITCH_BAND_AND_TUNE_STATUS,             // 
    RADIO_CMD_INTERNAL_TIMER_CHECK_RDS_STATUS,             // RDS状态检查定时器
} radio_internal_timer_cmd_t;







void service_radio_init(void);
void service_radio_exit(void);
radio_state_payload_t* service_radio_payload_alloc(void);
void service_radio_payload_free(radio_state_payload_t* payload);
radio_state_payload_t* service_radio_payload_copy(const radio_state_payload_t* src);

// 调试功能控制接口
void service_radio_set_debug(int enable);
void service_radio_set_info(int enable);

#endif