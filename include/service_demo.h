// --- START OF FILE service_demo.h ---

#ifndef __SERVICE_DEMO_H__
#define __SERVICE_DEMO_H__

#include <stdint.h>
#include <stdbool.h>
#include "infotainment_os_api.h"
#include "bus_router.h"

// --- 1. 定义服务的 ID 和 Address ---
#define SRC_DEMO_SERVICE   ((service_id_t)0x70000)      // 为新服务选择一个唯一的ID
#define ADDRESS_DEMO_SERVICE ((bus_router_address_t)1 << 6) // 选择一个未使用的地址位

// --- 2. 定义服务的内部状态 ---
typedef enum {
    DEMO_STATE_UNINITIALIZED, // 未初始化
    DEMO_STATE_IDLE,          // 空闲
    DEMO_STATE_ACTIVE,        // 正在工作
    DEMO_STATE_PROCESSING,    // 正在处理一个长时任务
    DEMO_STATE_ERROR,         // 错误状态
} demo_state_t;

// --- 3. 定义服务内部使用的定时器类型 ---
typedef enum {
    DEMO_TIMER_NONE = 0,
    DEMO_TIMER_TASK_TIMEOUT,    // 某个任务的超时定时器
    DEMO_TIMER_PERIODIC_UPDATE, // 周期性状态更新定时器
    DEMO_TIMER_TYPE_COUNT       // 内部定时器总数
} demo_internal_timer_type_t;

// --- 4. 定义服务的消息命令 (包括外部和内部) ---
typedef enum {
    DEMO_CMD_UNDEFINED = SRC_DEMO_SERVICE,

    // --- 来自其他服务 (如UI) 的请求 (CMD) ---
    DEMO_CMD_START_TASK,        // 请求开始一个任务
    DEMO_CMD_STOP_TASK,         // 请求停止一个任务
    DEMO_CMD_SET_PARAMETER,     // 设置参数

    // --- 内部命令，由定时器或回调触发 ---
    DEMO_CMD_INTERNAL_TASK_TIMEOUT,
    DEMO_CMD_INTERNAL_PERIODIC_UPDATE,

    // --- 发送给其他服务 (如UI) 的事件 (EVT) ---
    DEMO_EVT_READY,             // 服务已就绪
    DEMO_EVT_STATE_CHANGED,     // 服务状态已变更
    DEMO_EVT_TASK_STARTED,      // 任务已开始
    DEMO_EVT_TASK_COMPLETED,    // 任务已完成
    DEMO_EVT_ERROR_OCCURRED,    // 发生错误

    DEMO_CMD_MAX
} demo_message_cmd_t;

// --- 5. 定义服务的 Payload 结构体 ---
typedef struct {
    int         parameter;      // 示例参数
    char        data_string[32]; // 示例数据
    demo_state_t current_state; // 用于在事件中通报当前状态
} msg_payload_service_demo_t;

// --- 6. 公共 API ---
void service_demo_init(void);
void service_demo_exit(void);

// Payload 管理函数 (每个服务都应该有自己的一套)
msg_payload_service_demo_t* service_demo_payload_alloc(void);
void service_demo_payload_free(void* payload); // 注意是 void* 以便 find_free_func 使用
msg_payload_service_demo_t* service_demo_payload_copy(const void* src); // 注意是 const void*

// 调试接口
void service_demo_set_debug(int enable);
void service_demo_set_info(int enable);

#endif // __SERVICE_DEMO_H__