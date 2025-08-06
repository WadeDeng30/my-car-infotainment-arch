#ifndef __SERVICE_SYSTEM_H__
#define __SERVICE_SYSTEM_H__   
#include <stdint.h>
#include <infotainment_os_api.h>
#include <bus_router.h>
#include <system_api.h>



// 【新增】定义 System 服务内部使用的定时器类型
typedef enum {
    SYSTEM_TIMER_NONE = 0,
    SYSTEM_TIMER_STARTUP_DELAY,         // 系统启动延迟定时器
    SYSTEM_TIMER_SERVICE_CHECK,         // 服务状态检查定时器
    SYSTEM_TIMER_PING_TEST,             // ping测试定时器
    SYSTEM_TIMER_WATCHDOG,              // 看门狗定时器
    SYSTEM_TIMER_SHUTDOWN_DELAY,        // 关机延迟定时器
    SYSTEM_TIMER_UI_READY_CHECK,        // UI就绪检查定时器

    SYSTEM_TIMER_TYPE_COUNT             // 内部定时器的总数
} system_internal_timer_type_t;

// 【新增】系统服务内部命令
typedef enum {
    SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT = SRC_SYSTEM_SERVICE+0x8000,  // 启动超时
    SYSTEM_CMD_INTERNAL_SERVICE_CHECK,              // 服务检查
    SYSTEM_CMD_INTERNAL_PING_TEST_TIMEOUT,          // ping测试超时
    SYSTEM_CMD_INTERNAL_WATCHDOG_TIMEOUT,           // 看门狗超时
    SYSTEM_CMD_INTERNAL_SHUTDOWN_TIMEOUT,           // 关机超时
    SYSTEM_CMD_INTERNAL_UI_READY_TIMEOUT,           // UI就绪超时
} system_internal_cmd_t;


// 【新增】定义 system 服务的内部状态
typedef enum {
    SYSTEM_STATE_IDLE,           // 空闲状态 (例如，未播放，等待指令)
    SYSTEM_STATE_INIT,           // 初始化状态

} system_state_t;


typedef enum service_request_priority {
    SERVICE_REQUEST_RM_RDS_PRIORTY = 0,   // 低优先级请求
    SERVICE_REQUEST_DAB_ANN_PRIORTY,     // 普通优先级请求
    SERVICE_REQUEST_PHONE_VOICE_PRIORTY,       // 高优先级请求
    SERVICE_REQUEST_PHONE_CALL_PRIORTY,       // 高优先级请求
    SERVICE_REQUEST_CAMERM_PRIORTY,
    
    SERVICE_REQUEST_MAX_PRIORTY       // 高优先级请求
} service_request_priority_e;

void service_system_init(void);
void service_system_exit(void);
system_state_payload_t *service_system_payload_alloc(void);
void service_system_payload_free(system_state_payload_t* payload);
system_state_payload_t* service_system_payload_copy(const system_state_payload_t* src) ;

// 调试功能控制接口
void service_system_set_debug(int enable);
void service_system_set_info(int enable);

// 定时器管理便利函数（可选，供外部调用）
void service_system_start_startup_timer(uint32_t timeout_ms);
void service_system_start_watchdog_timer(uint32_t interval_ms);
void service_system_stop_all_timers(void);

#endif