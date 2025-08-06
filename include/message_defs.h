#ifndef __MESSAGE_DEFS_H__
#define __MESSAGE_DEFS_H__

#include <stdint.h>
#include <log.h>
#include <infotainment_os_api.h>



// 【新增】快速通道消息的类型
typedef enum {
    FAST_LANE_CMD_UI_OVERRIDE,      // UI覆盖指令 (如倒车)
    FAST_LANE_CMD_SHOW_CRITICAL_POPUP, // 显示严重故障灯
    // ... 其他紧急指令
} fast_lane_cmd_t;

// 【新增】快速通道消息的优先级
typedef enum {
    FAST_LANE_PRIO_NORMAL = 0,
    FAST_LANE_PRIO_HIGH,
    FAST_LANE_PRIO_CRITICAL,
} fast_lane_priority_t;

// 【新增】快速通道消息的结构体
typedef struct {
    fast_lane_cmd_t      command;
    fast_lane_priority_t priority;
    int32_t              param1; // 通用参数
    void* param2; // 通用指针参数
} fast_lane_msg_t;



// 【新增】消息优先级定义
typedef enum {
    MSG_PRIO_LOW = 0,       // 低优先级 (例如: 日志上报, 非关键的UI状态更新)
    MSG_PRIO_NORMAL,        // 普通优先级 (例如: 常规的用户操作响应, 数据刷新)
    MSG_PRIO_HIGH,          // 高优先级 (例如: 导航指令, 模式切换)
    //MSG_PRIO_CRITICAL,      // 关键优先级 (例如: 系统关机指令, 严重故障警告)
    MSG_PRIO_COUNT          // 用于计算优先级队列的数量
} message_priority_t;


// 消息来源/目的地ID
typedef enum {
    SRC_UNDEFINED = 0,
    SRC_UI = 0x10000,                 // UI层
    SRC_SYSTEM_SERVICE = 0x20000,     // 系统总管服务
    SRC_RADIO_SERVICE = 0x30000,      // 收音机服务
    SRC_CLIMATE_SERVICE = 0x40000,    // 空调服务
    SRC_INPUT_SERVICE = 0x50000,      // 输入服务
    SRC_TIMER_SERVICE = 0x60000,      // 定时器服务
    SRC_MSG_SERVICE = 0x70000,        // 消息转换服务
    SRC_MCU_SERVICE = 0x80000,        // MCU管理服务

} service_id_t;




/**
 * @brief 【新增】消息总线路由地址类型
 *
 * 这是一个位掩码，用于消息路由。每个服务有唯一的地址位。
 * 使用 uint64_t 可以支持最多64个服务，比32个更具扩展性。
 */
typedef uint64_t bus_router_address_t;

// 【新增】为每个服务定义路由地址常量
#define ADDRESS_NONE            ((bus_router_address_t)0)
#define ADDRESS_UI              ((bus_router_address_t)1 << 0)  // 1
#define ADDRESS_SYSTEM_SERVICE  ((bus_router_address_t)1 << 1)  // 2
#define ADDRESS_RADIO_SERVICE   ((bus_router_address_t)1 << 2)  // 4
#define ADDRESS_CLIMATE_SERVICE ((bus_router_address_t)1 << 3)  // 8
#define ADDRESS_TIMER_SERVICE   ((bus_router_address_t)1 << 4)  // 16
#define ADDRESS_MSG_SERVICE     ((bus_router_address_t)1 << 5)  // 32
#define ADDRESS_MCU_SERVICE     ((bus_router_address_t)1 << 6)  // 64
// ...
#define ADDRESS_BROADCAST       (~((bus_router_address_t)0)) // 广播到所有地址

// bus_router.c 内部可以定义一个带引用计数的 payload 包装器
typedef struct {
    void*   ptr;          // 指向真实的 payload
    int32_t ref_count;    // 引用计数
    void (*free_func)(void*); // 【重要】指向 payload 的专用释放函数
} ref_counted_payload_t;



// --- 核心消息结构 ---// 这是在消息总线中实际传递的结构体
typedef struct {
    service_id_t    source;         // 消息来源
    bus_router_address_t    destination;    // 消息目的地
    message_priority_t  priority; // 【新增】优先级字段
    ref_counted_payload_t* payload_wrapper; // 引用计数的 payload 包装器
    unsigned int   command;        // 消息命令/事件类型
    void* payload;        // 指向具体数据负载的指针，可以为NULL
    service_id_t    payload_source; // 【新增】payload的真正来源ID
    void* reserved;      // 保留字段，未来扩展使用 

} app_message_t;

#endif