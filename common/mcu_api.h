#ifndef __MCU_API_H__
#define __MCU_API_H__
#include <stdint.h>
#include <string.h>
#include <infotainment_os_api.h>
#include <bus_router.h>



// 消息命令/事件类型
typedef enum {
    MCU_CMD_UNDEFINED = SRC_MCU_SERVICE,
    
    // --- 电源管理命令 ---
    MCU_CMD_POWER_ON,               // 上电命令
    MCU_CMD_POWER_OFF,              // 下电命令
    MCU_CMD_POWER_RESET,            // 复位命令
    MCU_CMD_POWER_RESTART,          // 重启命令
    MCU_CMD_POWER_SLEEP,            // 休眠命令
    MCU_CMD_POWER_WAKEUP,           // 唤醒命令
    
    // --- 数据存储命令 ---
    MCU_CMD_DATA_SAVE,              // 数据保存命令
    MCU_CMD_DATA_LOAD,              // 数据加载命令
    MCU_CMD_DATA_BACKUP,            // 数据备份命令
    MCU_CMD_DATA_RESTORE,           // 数据恢复命令
    MCU_CMD_DATA_CLEAR,             // 数据清除命令
    
    // --- IO口扩展命令 ---
    MCU_CMD_IO_SET,                 // IO口设置命令
    MCU_CMD_IO_GET,                 // IO口读取命令
    MCU_CMD_IO_TOGGLE,              // IO口翻转命令
    MCU_CMD_IO_CONFIG,              // IO口配置命令
    
    // --- 跳线检测命令 ---
    MCU_CMD_JUMPER_DETECT,          // 跳线检测命令
    MCU_CMD_JUMPER_GET_STATUS,      // 获取跳线状态命令
    
    // --- MCU更新命令 ---
    MCU_CMD_UPDATE_BEGIN,           // 开始更新命令
    MCU_CMD_UPDATE_DATA,            // 更新数据命令
    MCU_CMD_UPDATE_END,             // 结束更新命令
    MCU_CMD_UPDATE_VERIFY,          // 验证更新命令
    
    // --- 事件通知 ---
    MCU_EVT_POWER_STATUS_CHANGED,   // 电源状态变化事件
    MCU_EVT_DATA_SAVED,             // 数据保存完成事件
    MCU_EVT_IO_STATUS_CHANGED,      // IO状态变化事件
    MCU_EVT_JUMPER_STATUS_CHANGED,  // 跳线状态变化事件
    MCU_EVT_UPDATE_PROGRESS,        // 更新进度事件
    MCU_EVT_ERROR_OCCURRED,         // 错误发生事件
    MCU_EVT_HEARTBEAT,              // 心跳事件
    
    MCU_MAX_CMD
} mcu_api_cmd_t;

// 【新增】电源状态数据结构
typedef struct {
    uint32_t power_state;           // 电源状态
    uint32_t voltage_level;         // 电压级别
    uint32_t current_level;         // 电流级别
    uint32_t temperature;           // 温度
    uint32_t timestamp;             // 时间戳
} mcu_power_status_t;

// 【新增】数据存储信息结构
typedef struct {
    uint32_t data_type;             // 数据类型
    uint32_t data_size;             // 数据大小
    uint32_t checksum;              // 校验和
    char data_path[128];            // 数据路径
    uint32_t timestamp;             // 时间戳
} mcu_data_info_t;

// 【新增】IO口状态结构
typedef struct {
    uint32_t io_mask;               // IO口掩码
    uint32_t io_value;              // IO口值
    uint32_t io_direction;          // IO口方向
    uint32_t io_config;             // IO口配置
    uint32_t timestamp;             // 时间戳
} mcu_io_status_t;

// 【新增】跳线状态结构
typedef struct {
    uint32_t jumper_mask;           // 跳线掩码
    uint32_t jumper_value;          // 跳线值
    uint32_t jumper_config;         // 跳线配置
    char jumper_description[64];    // 跳线描述
    uint32_t timestamp;             // 时间戳
} mcu_jumper_status_t;

// 【新增】MCU更新信息结构
typedef struct {
    uint32_t update_type;           // 更新类型
    uint32_t total_size;            // 总大小
    uint32_t current_size;          // 当前大小
    uint32_t package_index;         // 包索引
    uint32_t progress;              // 进度百分比
    char version[32];               // 版本信息
    uint32_t timestamp;             // 时间戳
} mcu_update_info_t;

// 【新增】MCU错误信息结构
typedef struct {
    uint32_t error_code;            // 错误代码
    uint32_t error_type;            // 错误类型
    char error_message[128];        // 错误消息
    uint32_t timestamp;             // 时间戳
} mcu_error_info_t;

// 【新增】MCU服务载荷结构体
typedef struct {
    union {
        mcu_power_status_t power_status;
        mcu_data_info_t data_info;
        mcu_io_status_t io_status;
        mcu_jumper_status_t jumper_status;
        mcu_update_info_t update_info;
        mcu_error_info_t error_info;
    } data;
    void *radio_reg_memory;
    void *init_reg_memory;
    
    uint32_t timestamp;             // 消息时间戳
    uint32_t sequence_id;           // 消息序列号
} mcu_state_payload_t;

#endif