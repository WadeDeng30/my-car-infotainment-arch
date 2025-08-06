#ifndef __SERVICE_MSG_H__
#define __SERVICE_MSG_H__

#include <stdint.h>
#include <infotainment_os_api.h>
#include <bus_router.h>
#include <service_timer.h>

// 【新增】定义 MSG 服务内部使用的定时器类型
typedef enum {
    MSG_TIMER_NONE = 0,
    MSG_TIMER_SYSTEM_CHECK,         // 系统状态检查定时器
    MSG_TIMER_MESSAGE_QUEUE_CHECK,  // 消息队列检查定时器
    MSG_TIMER_DRIVER_WATCHDOG,      // 驱动看门狗定时器
    MSG_TIMER_VEHICLE_STATUS_CHECK, // 车辆状态检查定时器

    MSG_TIMER_TYPE_COUNT            // 内部定时器的总数
} msg_internal_timer_type_t;

// 【新增】MSG服务内部命令
typedef enum {
    MSG_CMD_INTERNAL_SYSTEM_CHECK = 0x9000,     // 系统检查
    MSG_CMD_INTERNAL_QUEUE_CHECK,               // 队列检查
    MSG_CMD_INTERNAL_DRIVER_WATCHDOG,           // 驱动看门狗
    MSG_CMD_INTERNAL_VEHICLE_STATUS_CHECK,      // 车辆状态检查
} msg_internal_cmd_t;

// 消息命令/事件类型
typedef enum {
    MSG_CMD_UNDEFINED = SRC_MSG_SERVICE,
    
    // --- 系统消息转换 ---
    MSG_EVT_SYSTEM_READY,           // 系统就绪事件
    MSG_EVT_SYSTEM_SHUTDOWN,        // 系统关机事件
    MSG_EVT_SYSTEM_SLEEP,           // 系统休眠事件
    MSG_EVT_SYSTEM_WAKEUP,          // 系统唤醒事件
    MSG_EVT_SYSTEM_ERROR,           // 系统错误事件
    
    // --- 音频消息转换 ---
    MSG_EVT_AUDIO_VOLUME_CHANGE,    // 音量变化事件
    MSG_EVT_AUDIO_MUTE_CHANGE,      // 静音状态变化事件
    MSG_EVT_AUDIO_SOURCE_CHANGE,    // 音频源变化事件
    
    // --- 电源消息转换 ---
    MSG_EVT_POWER_ACC_ON,           // ACC电源开启
    MSG_EVT_POWER_ACC_OFF,          // ACC电源关闭
    MSG_EVT_POWER_LOW_BATTERY,      // 低电量警告
    MSG_EVT_POWER_CHARGING,         // 充电状态
    
    // --- 外设消息转换 ---
    MSG_EVT_USB_CONNECTED,          // USB设备连接
    MSG_EVT_USB_DISCONNECTED,       // USB设备断开
    MSG_EVT_SD_CARD_INSERTED,       // SD卡插入
    MSG_EVT_SD_CARD_REMOVED,        // SD卡移除
    
    // --- 车辆消息转换 ---
    MSG_EVT_REVERSE_GEAR_ON,        // 倒车档开启
    MSG_EVT_REVERSE_GEAR_OFF,       // 倒车档关闭
    MSG_EVT_HANDBRAKE_ON,           // 手刹开启
    MSG_EVT_HANDBRAKE_OFF,          // 手刹关闭

    // --- 网络消息转换 ---
    MSG_EVT_WIFI_CONNECTED,         // WiFi连接
    MSG_EVT_WIFI_DISCONNECTED,      // WiFi断开
    MSG_EVT_BLUETOOTH_PAIRED,       // 蓝牙配对
    MSG_EVT_BLUETOOTH_DISCONNECTED, // 蓝牙断开
    MSG_EVT_NETWORK_ERROR,          // 网络错误

    // --- 存储消息转换 ---
    MSG_EVT_STORAGE_MOUNTED,        // 存储设备挂载
    MSG_EVT_STORAGE_UNMOUNTED,      // 存储设备卸载
    MSG_EVT_STORAGE_FULL,           // 存储空间满
    MSG_EVT_STORAGE_ERROR,          // 存储错误

    MSG_MAX_CMD
} msg_message_cmd_t;

// 【新增】原始系统消息类型映射
typedef enum {
    RAW_MSG_TYPE_SYSTEM = 1,        // 系统消息
    RAW_MSG_TYPE_AUDIO = 2,         // 音频消息
    RAW_MSG_TYPE_POWER = 3,         // 电源消息
    RAW_MSG_TYPE_USB = 4,           // USB消息
    RAW_MSG_TYPE_VEHICLE = 5,       // 车辆消息
    RAW_MSG_TYPE_NETWORK = 6,       // 网络消息
    RAW_MSG_TYPE_STORAGE = 7,       // 存储消息
} raw_msg_type_t;

// 【新增】系统事件数据结构
typedef struct {
    uint32_t event_type;            // 事件类型
    uint32_t event_data;            // 事件数据
    uint32_t timestamp;             // 时间戳
    char description[64];           // 事件描述
} msg_system_event_t;

// 【新增】音频事件数据结构
typedef struct {
    uint32_t volume_level;          // 音量级别
    uint32_t mute_state;            // 静音状态
    uint32_t audio_source;          // 音频源
    uint32_t timestamp;             // 时间戳
} msg_audio_event_t;

// 【新增】电源事件数据结构
typedef struct {
    uint32_t power_state;           // 电源状态
    uint32_t battery_level;         // 电池电量
    uint32_t charging_state;        // 充电状态
    uint32_t timestamp;             // 时间戳
} msg_power_event_t;

// 【新增】USB事件数据结构
typedef struct {
    uint32_t device_type;           // 设备类型
    uint32_t device_id;             // 设备ID
    uint32_t connection_state;      // 连接状态
    char device_name[32];           // 设备名称
    uint32_t timestamp;             // 时间戳
} msg_usb_event_t;

// 【新增】车辆事件数据结构
typedef struct {
    uint32_t gear_state;            // 档位状态
    uint32_t handbrake_state;       // 手刹状态
    uint32_t speed;                 // 车速
    uint32_t timestamp;             // 时间戳
} msg_vehicle_event_t;

// 【新增】网络事件数据结构
typedef struct {
    uint32_t network_type;          // 网络类型 (WiFi/蓝牙/4G等)
    uint32_t connection_state;      // 连接状态
    uint32_t signal_strength;       // 信号强度
    char network_name[32];          // 网络名称
    uint32_t timestamp;             // 时间戳
} msg_network_event_t;

// 【新增】存储事件数据结构
typedef struct {
    uint32_t storage_type;          // 存储类型 (SD卡/USB/内部存储)
    uint32_t storage_state;         // 存储状态 (插入/移除/满/错误)
    uint32_t total_space;           // 总空间
    uint32_t free_space;            // 剩余空间
    char storage_path[64];          // 存储路径
    uint32_t timestamp;             // 时间戳
} msg_storage_event_t;

// 【新增】MSG服务载荷结构体
typedef struct {
    raw_msg_type_t msg_type;        // 消息类型
    uint32_t raw_msg_id;            // 原始消息ID
    uint32_t raw_msg_data;          // 原始消息数据

    union {
        msg_system_event_t system_event;
        msg_audio_event_t audio_event;
        msg_power_event_t power_event;
        msg_usb_event_t usb_event;
        msg_vehicle_event_t vehicle_event;
        msg_network_event_t network_event;
        msg_storage_event_t storage_event;
    } event_data;
    
    uint32_t timestamp;             // 消息时间戳
    uint32_t sequence_id;           // 消息序列号
} msg_payload_service_msg_t;

// --- 函数声明 ---
void service_msg_init(void);
void service_msg_exit(void);

// Payload 管理函数
msg_payload_service_msg_t* service_msg_payload_alloc(void);
void service_msg_payload_free(msg_payload_service_msg_t* payload);
msg_payload_service_msg_t* service_msg_payload_copy(const msg_payload_service_msg_t* src);

// 调试功能控制接口
void service_msg_set_debug(int enable);
void service_msg_set_info(int enable);

// 消息转换和分发接口
int service_msg_register_raw_message_hook(void);
int service_msg_unregister_raw_message_hook(void);
int service_msg_convert_and_dispatch(uint32_t raw_msg_type, uint32_t raw_msg_id, uint32_t raw_msg_data);

// 定时器管理便利函数
void service_msg_start_system_check_timer(uint32_t interval_ms);
void service_msg_start_driver_watchdog_timer(uint32_t timeout_ms);
void service_msg_start_vehicle_status_check_timer(uint32_t interval_ms);
void service_msg_stop_all_timers(void);

// 测试和调试函数
void service_msg_test_simulate_raw_messages(void);
void service_msg_dump_statistics(void);
int32_t msg_raw_system_hook(void* msg);

#endif // __SERVICE_MSG_H__
