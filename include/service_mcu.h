#ifndef __SERVICE_MCU_H__
#define __SERVICE_MCU_H__

#include <stdint.h>
#include <infotainment_os_api.h>
#include <bus_router.h>
#include <service_timer.h>
#include <kapi.h>  // 【新增】包含 __awos_date_t 和 __awos_time_t 类型定义
#include <mcu_api.h>


// 【新增】定义 MCU 服务内部使用的定时器类型
typedef enum {
    MCU_TIMER_NONE = 0,
    MCU_TIMER_POWER_CHECK,          // 电源状态检查定时器
    MCU_TIMER_DATA_SAVE,            // 数据保存定时器
    MCU_TIMER_IO_MONITOR,           // IO口监控定时器
    MCU_TIMER_JUMPER_DETECT,        // 跳线检测定时器
    MCU_TIMER_WATCHDOG,             // MCU看门狗定时器
    MCU_TIMER_HEARTBEAT,            // MCU心跳定时器
    MCU_TIMER_BACKUP_CHECK,         // 备份检查定时器
    
    MCU_TIMER_TYPE_COUNT            // 内部定时器的总数
} mcu_internal_timer_type_t;

// 【新增】MCU服务内部命令
typedef enum {
    MCU_CMD_INTERNAL_POWER_CHECK = 0xA000,     // 电源检查
    MCU_CMD_INTERNAL_DATA_SAVE,                // 数据保存
    MCU_CMD_INTERNAL_IO_MONITOR,               // IO监控
    MCU_CMD_INTERNAL_JUMPER_DETECT,            // 跳线检测
    MCU_CMD_INTERNAL_WATCHDOG,                 // 看门狗
    MCU_CMD_INTERNAL_HEARTBEAT,                // 心跳
    MCU_CMD_INTERNAL_BACKUP_CHECK,             // 备份检查
} mcu_internal_cmd_t;

// MCU服务状态枚举
typedef enum {
    MCU_STATE_IDLE = 0,             // 空闲状态
    MCU_STATE_INITIALIZING,         // 初始化状态
    MCU_STATE_RUNNING,              // 运行状态
    MCU_STATE_POWER_SAVING,         // 省电状态
    MCU_STATE_UPDATING,             // 更新状态
    MCU_STATE_ERROR,                // 错误状态
    MCU_STATE_SHUTDOWN,             // 关机状态
} mcu_state_t;


// --- 函数声明 ---
void service_mcu_init(void);
void service_mcu_exit(void);

// Payload 管理函数
mcu_state_payload_t* service_mcu_payload_alloc(void);
void service_mcu_payload_free(mcu_state_payload_t* payload);
mcu_state_payload_t* service_mcu_payload_copy(const mcu_state_payload_t* src);

// 调试功能控制接口
void service_mcu_set_debug(int enable);
void service_mcu_set_info(int enable);

// 电源管理接口
int service_mcu_power_on(void);
int service_mcu_power_off(void);
int service_mcu_power_reset(void);
int service_mcu_power_restart(uint8_t flag);
int service_mcu_get_power_status(mcu_power_status_t* status);

// 数据存储接口
int service_mcu_save_data(uint32_t data_type);
int service_mcu_load_data(uint32_t data_type, void* buffer, uint32_t size);
int service_mcu_backup_data(void);
int service_mcu_restore_data(void);

// IO口扩展接口
int service_mcu_set_io(uint32_t io_mask, uint32_t io_value);
int service_mcu_get_io(uint32_t io_mask, uint32_t* io_value);
int service_mcu_toggle_io(uint32_t io_mask);
int service_mcu_config_io(uint32_t io_mask, uint32_t config);

// 跳线检测接口
int service_mcu_detect_jumper(void);
int service_mcu_get_jumper_status(mcu_jumper_status_t* status);

// MCU更新接口
int service_mcu_update_begin(uint32_t data_length);
int service_mcu_update_data(uint16_t package_index, uint8_t data_length, uint8_t* data);
int service_mcu_update_end(void);
int service_mcu_update_verify(void);

// 定时器管理便利函数
void service_mcu_start_power_check_timer(uint32_t interval_ms);
void service_mcu_start_data_save_timer(uint32_t interval_ms);
void service_mcu_start_io_monitor_timer(uint32_t interval_ms);
void service_mcu_start_jumper_detect_timer(uint32_t interval_ms);
void service_mcu_stop_all_timers(void);

// 状态管理函数
mcu_state_t service_mcu_get_state(void);
const char* service_mcu_state_to_string(mcu_state_t state);

// 额外的便利函数 - 直接调用 dsk_mcu.c 中的函数
int service_mcu_send_ready(void);
int service_mcu_send_date_time(__awos_date_t date, __awos_time_t time);
int service_mcu_send_tuner_type(uint8_t tuner_type);
int service_mcu_send_current_source(uint8_t source);
int service_mcu_set_beep_on_off(uint8_t on_off);
int service_mcu_set_beep_setting(uint16_t time);
int service_mcu_send_power_off_status(uint8_t status);
#ifdef MCU_FLASH_SAVE_ENABLE
int service_mcu_flush_flash_memory(void);
#endif

#endif // __SERVICE_MCU_H__
