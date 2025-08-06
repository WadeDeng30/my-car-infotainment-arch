// --- START OF FILE service_timer.h ---

#ifndef SERVICE_TIMER_H
#define SERVICE_TIMER_H

#include "message_defs.h" // 包含消息总线的基础定义
#include "infotainment_os_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================
// 常量定义
// ================================

#define TIMER_SERVICE_MAX_TIMERS        256      // 最大定时器数量
#define TIMER_SERVICE_MIN_DURATION_MS   1       // 最小定时间隔(毫秒)
#define TIMER_SERVICE_MAX_DURATION_MS   (24*60*60*1000)  // 最大定时间隔(24小时)
#define TIMER_SERVICE_INVALID_ID        0       // 无效定时器ID

// ================================
// 错误码定义
// ================================

typedef enum {
    TIMER_ERROR_SUCCESS = 0,            // 成功
    TIMER_ERROR_INVALID_PARAM = -1,     // 无效参数
    TIMER_ERROR_NO_MEMORY = -2,         // 内存不足
    TIMER_ERROR_MAX_TIMERS = -3,        // 达到最大定时器数量
    TIMER_ERROR_NOT_FOUND = -4,         // 定时器未找到
    TIMER_ERROR_SYSTEM_ERROR = -5,      // 系统错误
    TIMER_ERROR_SERVICE_NOT_READY = -6  // 服务未就绪
} timer_error_t;

// ================================
// 定时器状态定义
// ================================

typedef enum {
    TIMER_STATE_IDLE = 0,               // 空闲状态
    TIMER_STATE_RUNNING,                // 运行状态
    TIMER_STATE_EXPIRED,                // 已过期状态
    TIMER_STATE_STOPPED                 // 已停止状态
} timer_state_t;

// ================================
// 定时器服务的内部命令
// ================================
typedef enum {
    TIMER_CMD_UNDEFINED = SRC_TIMER_SERVICE,
    TIMER_CMD_START,      // 内部命令：启动一个新定时器
    TIMER_CMD_STOP,       // 内部命令：停止一个已有的定时器
    TIMER_CMD_QUERY,      // 内部命令：查询定时器状态
    TIMER_CMD_MODIFY,     // 内部命令：修改定时器参数
} timer_message_cmd_t;

// --- 3. 定时器类型 ---
typedef enum {
    TIMER_TYPE_ONE_SHOT,  // 单次定时器
    TIMER_TYPE_PERIODIC   // 周期性定时器
} timer_type_t;

// --- 4. 启动定时器的 payload 结构体 (用于内部消息) ---
typedef struct {
    uint32_t             timer_id;          // [OUT] 服务内部生成的ID
    uint32_t             duration_ms;       // 定时时长 (毫秒)
    timer_type_t         type;              // 定时器类型 (单次/周期)
    bus_router_address_t target_address;    // 定时器到期后，消息要发往的目标地址
    unsigned int         target_command;    // 定时器到期后，发送的消息命令
    void*                user_data;         // 到期时需要回传的用户数据(payload)
    service_id_t         user_data_source;  // user_data的来源，用于查找释放函数
} msg_payload_timer_start_t;

// --- 5. 停止定时器的 payload 结构体 (用于内部消息) ---
typedef struct {
    uint32_t timer_id; // 要停止的定时器的唯一ID
} msg_payload_timer_stop_t;

// --- 6. 查询定时器的 payload 结构体 ---
typedef struct {
    uint32_t timer_id;                  // 要查询的定时器ID
    timer_state_t state;                // [OUT] 定时器状态
    uint32_t remaining_ms;              // [OUT] 剩余时间(毫秒)
    timer_type_t type;                  // [OUT] 定时器类型
} msg_payload_timer_query_t;

// --- 7. 定时器统计信息结构体 ---
typedef struct {
    uint32_t total_timers;              // 总定时器数量
    uint32_t active_timers;             // 活跃定时器数量
    uint32_t expired_count;             // 过期次数统计
    uint32_t max_timers_used;           // 历史最大使用数量
    uint32_t memory_usage_bytes;        // 内存使用量
} timer_service_stats_t;

// ================================
// 公共API
// ================================

/**
 * @brief 初始化并启动定时器服务线程
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t service_timer_init(void);

/**
 * @brief 停止并清理定时器服务
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t service_timer_exit(void);

/**
 * @brief 检查定时器服务是否就绪
 * @return true表示就绪，false表示未就绪
 */
bool service_timer_is_ready(void);

/**
 * @brief 启动一个定时器 (供其他服务调用)
 * @param duration_ms      定时间隔 (毫秒)
 * @param type             定时器类型
 * @param target_address   到期后消息的目标地址
 * @param target_command   到期后消息的命令
 * @param user_data        到期后消息携带的payload，定时器服务将接管其生命周期
 * @param user_data_source 【重要】user_data 的来源服务ID，用于查找正确的释放/拷贝函数。
 *                         如果 user_data 为 NULL，此参数应为 SRC_UNDEFINED。
 * @return 成功则返回 timer_id (大于0)，失败返回 0。
 */
uint32_t timer_start(
    uint32_t duration_ms,
    timer_type_t type,
    bus_router_address_t target_address,
    unsigned int target_command,
    void* user_data,
    service_id_t user_data_source // <-- 恢复这个参数
);


/**
 * @brief 【新增】一个更方便的宏，用于启动不带 payload 的定时器
 */
#define timer_start_simple(duration, timer_type, target_addr, command) \
    timer_start(duration, timer_type, target_addr, command, NULL, SRC_UNDEFINED)

/**
 * @brief 停止一个定时器 (供其他服务调用)
 * @param timer_id 调用 timer_start 时返回的ID
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t timer_stop(uint32_t timer_id);

/**
 * @brief 查询定时器状态
 * @param timer_id 定时器ID
 * @param state [OUT] 定时器状态
 * @param remaining_ms [OUT] 剩余时间(毫秒)
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t timer_query(uint32_t timer_id, timer_state_t* state, uint32_t* remaining_ms);

/**
 * @brief 修改定时器间隔
 * @param timer_id 定时器ID
 * @param new_duration_ms 新的定时间隔(毫秒)
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t timer_modify(uint32_t timer_id, uint32_t new_duration_ms);

/**
 * @brief 获取定时器服务统计信息
 * @param stats [OUT] 统计信息结构体指针
 * @return TIMER_ERROR_SUCCESS表示成功，其他值表示错误
 */
timer_error_t timer_get_stats(timer_service_stats_t* stats);

/**
 * @brief 打印所有定时器状态(调试用)
 */
void timer_dump_all(void);

/**
 * @brief 设置定时器服务调试信息开关
 * @param enable 1开启，0关闭
 */
void service_timer_set_debug(int enable);

/**
 * @brief 设置定时器服务一般信息开关
 * @param enable 1开启，0关闭
 */
void service_timer_set_info(int enable);

#ifdef __cplusplus
}
#endif

#endif // SERVICE_TIMER_H