// --- START OF FILE service_audio.h ---

#ifndef __SERVICE_AUDIO_H__
#define __SERVICE_AUDIO_H__

#include <stdint.h>
#include <stdbool.h>
#include "infotainment_os_api.h"
#include "bus_router.h"

// --- 1. 定义服务的 ID 和 Address ---
#define SRC_AUDIO_SERVICE     ((service_id_t)0x90000)          // 音频服务ID
#define ADDRESS_AUDIO_SERVICE ((bus_router_address_t)1 << 7)   // 音频服务路由地址

// --- 2. 定义服务的内部状态 ---
typedef enum {
    AUDIO_STATE_UNINITIALIZED, // 未初始化
    AUDIO_STATE_READY,         // 已就绪
    AUDIO_STATE_ERROR,         // 错误状态
} audio_state_t;

// --- 3. 定义服务的消息命令 (包括事件) ---
typedef enum {
    AUDIO_CMD_UNDEFINED = SRC_AUDIO_SERVICE,

    // --- 外部命令 ---
    AUDIO_CMD_FOCUS_REQUEST,   // 请求音频焦点
    AUDIO_CMD_FOCUS_RELEASE,   // 释放音频焦点
    AUDIO_CMD_SET_MIX,         // 设置混音参数
    AUDIO_CMD_SET_EQ,          // 设置EQ
    AUDIO_CMD_SET_XBASS,       // 设置XBass
    AUDIO_CMD_SET_DELAY,       // 设置延迟

    // --- 事件 ---
    AUDIO_EVT_READY,           // 服务已就绪
    AUDIO_EVT_FOCUS_CHANGED,   // 焦点已改变
    AUDIO_EVT_DSP_UPDATED,     // DSP设置已更新

    AUDIO_CMD_MAX,
} audio_message_cmd_t;

// --- 4. 定义服务的 Payload 结构体 ---
typedef struct {
    service_id_t  focus_owner;                 // 当前焦点持有者
    int           mix_level;                   // 混音电平
    int8_t        eq[8];                       // 8段EQ设置
    bool          xbass_enabled;               // XBass开关
    uint32_t      delay_ms;                    // 延迟时间
    audio_state_t current_state;               // 服务状态
} msg_payload_service_audio_t;

// --- 5. 公共 API ---
void service_audio_init(void);
void service_audio_exit(void);

// Payload 管理函数
msg_payload_service_audio_t* service_audio_payload_alloc(void);
void service_audio_payload_free(void* payload);
msg_payload_service_audio_t* service_audio_payload_copy(const void* src);

// 调试接口
void service_audio_set_debug(int enable);
void service_audio_set_info(int enable);

#endif // __SERVICE_AUDIO_H__

// --- END OF FILE service_audio.h ---

