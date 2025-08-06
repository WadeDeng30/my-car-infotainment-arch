#include "service_msg.h"
#include "bus_router.h"
#include <msg_emit.h>
#include <kapi.h>
#include <desktop_api.h>

// --- 调试功能相关定义 ---
static int g_msg_service_debug = 0;  // 调试开关，默认关闭
static int g_msg_service_info = 0;   // 信息开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define MSG_DEBUG(fmt, ...) \
    do { \
        if (g_msg_service_debug) { \
            printf("[MsgService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define MSG_INFO(fmt, ...) \
    do { \
        if (g_msg_service_info) { \
            printf("[MsgService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define MSG_ERROR(fmt, ...) \
    printf("[MsgService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

// --- 调试宏 ---
#define MSG_HOOK_DEBUG(fmt, ...) \
    do { \
        printf("[MsgHook-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
    } while(0)

#define MSG_HOOK_ERROR(fmt, ...) \
    printf("[MsgHook-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)




// --- 内部定义 ---
#define MSG_QUEUE_SIZE 128
#define MSG_THREAD_STACK_SIZE (1024 * 8)
#define MSG_SERVICE_NAME "ServiceMsg"

// 【新增】定义一个结构体来保存单个定时器的信息
typedef struct {
    msg_internal_timer_type_t type;     // 定时器的用途
    uint32_t                  timer_id; // 从 service_timer 返回的ID
} msg_active_timer_info_t;

// 【新增】MSG 服务上下文结构体
typedef struct {
    // 使用一个固定大小的数组来管理所有活动定时器
    msg_active_timer_info_t active_timers[MSG_TIMER_TYPE_COUNT];
    
    // 消息统计
    uint32_t total_messages_received;
    uint32_t total_messages_converted;
    uint32_t total_messages_dispatched;
    uint32_t conversion_errors;
    uint32_t sequence_counter;
    
    // 服务状态
    bool service_ready;
    bool hook_registered;
} msg_service_context_t;

// --- 全局变量 ---
static void* g_msg_queue_handle = NULL;
static int g_msg_thread_handle = 0;
static void* g_msg_mutex = NULL;
static void* g_msg_semaphore = NULL;
static msg_service_context_t g_msg_ctx;

// Payload 管理
static int service_msg_payload_use_count = 0;

// --- 内部函数声明 ---
static void msg_thread_entry(void* p_arg);
static void msg_event_handler(app_message_t* msg);
static void msg_timer_start(msg_internal_timer_type_t timer_type, uint32_t duration_ms, 
                           timer_type_t type, unsigned int target_command, void* user_data);
static void msg_timer_stop(msg_internal_timer_type_t timer_type);
static void msg_timer_stop_all(void);
static void msg_handle_internal_timer_timeout(unsigned int command);
static int msg_convert_raw_message(uint32_t raw_msg_type, uint32_t raw_msg_id, uint32_t raw_msg_data, 
                                   msg_message_cmd_t* out_cmd, msg_payload_service_msg_t* out_payload);
static void msg_dispatch_converted_message(msg_message_cmd_t cmd, msg_payload_service_msg_t* payload);

// --- 外部控制调试开关的函数 ---
void service_msg_set_debug(int enable) {
    g_msg_service_debug = enable;
    MSG_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void service_msg_set_info(int enable) {
    g_msg_service_info = enable;
    MSG_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

// --- Payload 管理函数 ---
msg_payload_service_msg_t* service_msg_payload_alloc(void) {
    unsigned char err;
    if (g_msg_semaphore)
        infotainment_sem_wait(g_msg_semaphore, 0, &err);
    if (service_msg_payload_use_count >= MSG_QUEUE_SIZE) {
        if (g_msg_semaphore)
            infotainment_sem_post(g_msg_semaphore);
        MSG_ERROR("Payload allocation limit reached");
        return NULL;
    }
    
    msg_payload_service_msg_t* service_msg_payload = (msg_payload_service_msg_t*)infotainment_malloc(sizeof(msg_payload_service_msg_t));
    if (!service_msg_payload) {
        MSG_ERROR("Failed to allocate payload memory");
        if (g_msg_semaphore)
            infotainment_sem_post(g_msg_semaphore);
        return NULL;
    }
    
    memset(service_msg_payload, 0, sizeof(msg_payload_service_msg_t));
    service_msg_payload_use_count++;
    service_msg_payload->sequence_id = ++g_msg_ctx.sequence_counter;
    service_msg_payload->timestamp = infotainment_get_ticks();
    
    if (g_msg_semaphore)
        infotainment_sem_post(g_msg_semaphore);
    
    return service_msg_payload;
}

void service_msg_payload_free(msg_payload_service_msg_t* payload) {
    if (!payload) return;
    
    unsigned char err;
    if (g_msg_semaphore)
        infotainment_sem_wait(g_msg_semaphore, 0, &err);
    
    infotainment_free(payload);
    service_msg_payload_use_count--;
    
    if (g_msg_semaphore)
        infotainment_sem_post(g_msg_semaphore);
    MSG_DEBUG("Payload memory freed");
}

msg_payload_service_msg_t* service_msg_payload_copy(const msg_payload_service_msg_t* src) {
    if (!src) return NULL;
    
    msg_payload_service_msg_t* copy = service_msg_payload_alloc();
    if (copy) {
        memcpy(copy, src, sizeof(msg_payload_service_msg_t));
        copy->sequence_id = ++g_msg_ctx.sequence_counter; // 新的序列号
        copy->timestamp = infotainment_get_ticks();       // 新的时间戳
    }
    return copy;
}

// --- 主要接口函数 ---
void service_msg_init(void) {
    unsigned char err = 0;
    MSG_INFO("Initializing message service...");

    if (g_msg_ctx.service_ready) {
        MSG_INFO("Message service already initialized");
        return;
    }
    
    // 初始化服务上下文
    memset(&g_msg_ctx, 0, sizeof(msg_service_context_t));
    for (int i = 0; i < MSG_TIMER_TYPE_COUNT; i++) {
        g_msg_ctx.active_timers[i].type = MSG_TIMER_NONE;
        g_msg_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
    }
    
    // 创建信号量
    g_msg_semaphore = infotainment_sem_create(1);
    if (!g_msg_semaphore) {
        MSG_ERROR("Failed to create semaphore");
        return;
    }
    
    // 创建互斥锁
    g_msg_mutex = infotainment_mutex_create(0, &err);
    if (!g_msg_mutex || err != 0) {
        MSG_ERROR("Failed to create mutex, error: %d", err);
        infotainment_sem_delete(g_msg_semaphore, &err);
        g_msg_semaphore = NULL;
        return;
    }
    
    // 创建线程
    g_msg_thread_handle = infotainment_thread_create(
        msg_thread_entry, NULL, TASK_PROI_LEVEL2,
        MSG_THREAD_STACK_SIZE, MSG_SERVICE_NAME
    );
    if (g_msg_thread_handle <= 0) {
        MSG_ERROR("Failed to create thread");
        infotainment_mutex_delete(g_msg_mutex, &err);
        infotainment_sem_delete(g_msg_semaphore, &err);
        g_msg_mutex = NULL;
        g_msg_semaphore = NULL;
        return;
    }
    
    MSG_INFO("Message service initialized successfully");
}

void service_msg_exit(void) {
    unsigned char err = 0;
    MSG_INFO("Exiting message service...");
    
    if (!g_msg_ctx.service_ready) {
        return;
    }
    
    // 停止所有定时器
    msg_timer_stop_all();
    
    // 注销消息钩子
    service_msg_unregister_raw_message_hook();
    
    // 注销服务
    bus_router_unregister_service(SRC_MSG_SERVICE);
    
    // 请求线程退出
    if (g_msg_thread_handle > 0) {
        MSG_DEBUG("Requesting message thread deletion...");
        infotainment_thread_delete_req(g_msg_thread_handle);
        g_msg_thread_handle = 0;
    }
    
    // 清理资源
    if (g_msg_queue_handle) {
        infotainment_queue_delete(g_msg_queue_handle, &err);
        g_msg_queue_handle = NULL;
    }
    
    if (g_msg_mutex) {
        infotainment_mutex_delete(g_msg_mutex, &err);
        g_msg_mutex = NULL;
    }
    
    if (g_msg_semaphore) {
        infotainment_sem_delete(g_msg_semaphore, &err);
        g_msg_semaphore = NULL;
    }
    
    // 重置上下文
    memset(&g_msg_ctx, 0, sizeof(msg_service_context_t));
    
    MSG_INFO("Message service cleanup complete");
}

// --- 线程入口函数 ---
static void msg_thread_entry(void* p_arg) {
    unsigned char err = 0;
	__memit_hook_t  emit_hook;
    MSG_INFO("Message service thread started");

    // 1. 创建消息队列
    g_msg_queue_handle = infotainment_queue_create(MSG_QUEUE_SIZE);
    if (!g_msg_queue_handle) {
        MSG_ERROR("Failed to create message queue");
        infotainment_thread_delete(g_msg_thread_handle);
        return;
    }
    MSG_INFO("Message queue created with handle %p", g_msg_queue_handle);

    // 2. 注册服务到消息总线路由器
    if (bus_router_register_service(SRC_MSG_SERVICE, ADDRESS_MSG_SERVICE, g_msg_queue_handle, "Message service",service_msg_payload_free,service_msg_payload_copy) < 0) {
        MSG_ERROR("Failed to register service with bus router");
        infotainment_queue_delete(g_msg_queue_handle, &err);
        infotainment_thread_delete(g_msg_thread_handle);
        return;
    }

    // 3. 注册原始消息钩子
    if (service_msg_register_raw_message_hook() != 0) {
        MSG_ERROR("Failed to register raw message hook");
    }

    // 4. 启动内部定时器
    MSG_INFO("Starting message service timers...");
    msg_timer_start(MSG_TIMER_SYSTEM_CHECK, 10000, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_SYSTEM_CHECK, NULL);
    msg_timer_start(MSG_TIMER_MESSAGE_QUEUE_CHECK, 5000, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_QUEUE_CHECK, NULL);
    msg_timer_start(MSG_TIMER_DRIVER_WATCHDOG, 30000, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_DRIVER_WATCHDOG, NULL);
    msg_timer_start(MSG_TIMER_VEHICLE_STATUS_CHECK, 15000, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_VEHICLE_STATUS_CHECK, NULL);

    g_msg_ctx.service_ready = true;


    MSG_INFO("Message service ready");

    // 5. 主消息循环
    while (1) {
        // 检查是否有线程删除请求
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            MSG_DEBUG("Message service thread delete request received");
            goto exit;
        }

        // 获取消息
        app_message_t* received_msg = (app_message_t*)infotainment_queue_pend(g_msg_queue_handle, 0,&err);
        if (received_msg && err == OS_NO_ERR) {
            MSG_DEBUG("收到消息 (来源:0x%x, 命令:0x%x)", received_msg->source, received_msg->command);
            msg_event_handler(received_msg);

            // 释放消息内存
            if (received_msg->payload) {
                free_ref_counted_payload((ref_counted_payload_t *)received_msg->payload);
            }
            infotainment_free(received_msg);
            received_msg = NULL;
            MSG_DEBUG("Message processed and memory freed");
        }
    }

exit:
    // 清理线程资源
    MSG_DEBUG("Message service thread cleanup started");

    // 清空消息队列中的所有消息，防止内存泄漏
    if (g_msg_queue_handle) {
        void* msg = NULL;
        unsigned char queue_err = 0;

        while ((msg = infotainment_queue_get(g_msg_queue_handle, &queue_err)) && queue_err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg && app_msg->payload) {
                service_msg_payload_free((msg_payload_service_msg_t*)app_msg->payload);
                app_msg->payload = NULL;
            }
            if (app_msg) {
                infotainment_free(app_msg);
            }
            MSG_DEBUG("Freed queued message during cleanup");
        }
    }

    MSG_DEBUG("Message service thread cleanup completed");

    // 删除线程自身
    infotainment_thread_delete(g_msg_thread_handle);
}

// --- 消息事件处理函数 ---
static void msg_event_handler(app_message_t* msg) {
    if (!msg) {
        MSG_ERROR("Received invalid message");
        return;
    }

    MSG_DEBUG("Processing message from source 0x%x, command 0x%x", msg->source, msg->command);

    if (msg->command == CMD_PING_REQUEST || msg->command == CMD_PING_BROADCAST) {
        service_handle_ping_request(msg);
        return;
    }
    else if (msg->command == CMD_PONG_RESPONSE) {
        service_handle_pong_response(msg);
        return;
    }



    switch (msg->source) {
        case SRC_TIMER_SERVICE: {
            // 处理来自定时器服务的消息
            if (msg->command >= MSG_CMD_INTERNAL_SYSTEM_CHECK &&
                msg->command <= MSG_CMD_INTERNAL_DRIVER_WATCHDOG) {
                msg_handle_internal_timer_timeout(msg->command);
            }
            break;
        }

        case SRC_MSG_SERVICE: {
            // 处理来自自己的内部消息（定时器超时等）
            if (msg->command >= MSG_CMD_INTERNAL_SYSTEM_CHECK &&
                msg->command <= MSG_CMD_INTERNAL_DRIVER_WATCHDOG) {
                msg_handle_internal_timer_timeout(msg->command);
            }
            break;
        }

        default: {
            MSG_ERROR("Received message from unknown source 0x%x", msg->source);
            break;
        }
    }
}

// --- 内部定时器超时处理函数 ---
static void msg_handle_internal_timer_timeout(unsigned int command) {
    MSG_DEBUG("Handling internal timer timeout: command=0x%x", command);

    switch (command) {
        case MSG_CMD_INTERNAL_SYSTEM_CHECK:
            MSG_DEBUG("Performing system check");
            // 检查系统状态，统计信息等
            MSG_INFO("Message stats: received=%u, converted=%u, dispatched=%u, errors=%u",
                    g_msg_ctx.total_messages_received,
                    g_msg_ctx.total_messages_converted,
                    g_msg_ctx.total_messages_dispatched,
                    g_msg_ctx.conversion_errors);
            break;

        case MSG_CMD_INTERNAL_QUEUE_CHECK:
            MSG_DEBUG("Checking message queue status");
            // 检查消息队列状态
            break;

        case MSG_CMD_INTERNAL_DRIVER_WATCHDOG:
            MSG_DEBUG("Driver watchdog check");
            // 驱动看门狗检查
            break;

        case MSG_CMD_INTERNAL_VEHICLE_STATUS_CHECK:
            MSG_DEBUG("Vehicle status check");
            // 车辆状态检查处理
            break;

        default:
            MSG_ERROR("Unknown internal timer command: 0x%x", command);
            break;
    }
}

// --- 原始消息转换函数 ---
static int msg_convert_raw_message(uint32_t raw_msg_type, uint32_t raw_msg_id, uint32_t raw_msg_data,
                                   msg_message_cmd_t* out_cmd, msg_payload_service_msg_t* out_payload) {
    if (!out_cmd || !out_payload) {
        MSG_ERROR("Invalid parameters for message conversion");
        return -1;
    }

    // 清空输出结构
    memset(out_payload, 0, sizeof(msg_payload_service_msg_t));
    *out_cmd = MSG_CMD_UNDEFINED;

    // 填充基本信息
    out_payload->raw_msg_id = raw_msg_id;
    out_payload->raw_msg_data = raw_msg_data;
    out_payload->timestamp = infotainment_get_ticks();
    out_payload->sequence_id = ++g_msg_ctx.sequence_counter;

    MSG_DEBUG("Converting raw message: type=%u, id=0x%x, data=0x%x", raw_msg_type, raw_msg_id, raw_msg_data);

    switch (raw_msg_type) {
        case RAW_MSG_TYPE_SYSTEM: {
            out_payload->msg_type = RAW_MSG_TYPE_SYSTEM;

            // 解析系统消息
            out_payload->event_data.system_event.event_type = raw_msg_id;
            out_payload->event_data.system_event.event_data = raw_msg_data;
            out_payload->event_data.system_event.timestamp = out_payload->timestamp;
            snprintf(out_payload->event_data.system_event.description,
                    sizeof(out_payload->event_data.system_event.description),
                    "System event: type=0x%x, data=0x%x", raw_msg_id, raw_msg_data);

            // 根据系统事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_SYSTEM_READY; break;
                case 0x02: *out_cmd = MSG_EVT_SYSTEM_SHUTDOWN; break;
                case 0x03: *out_cmd = MSG_EVT_SYSTEM_SLEEP; break;
                case 0x04: *out_cmd = MSG_EVT_SYSTEM_WAKEUP; break;
                default: *out_cmd = MSG_EVT_SYSTEM_ERROR; break;
            }

            MSG_DEBUG("Converted system event: type=0x%x, cmd=0x%x", raw_msg_id, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_AUDIO: {
            out_payload->msg_type = RAW_MSG_TYPE_AUDIO;

            // 解析音频消息
            out_payload->event_data.audio_event.volume_level = (raw_msg_data & 0xFF);
            out_payload->event_data.audio_event.mute_state = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.audio_event.audio_source = ((raw_msg_data >> 16) & 0xFF);
            out_payload->event_data.audio_event.timestamp = out_payload->timestamp;

            // 根据音频事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_AUDIO_VOLUME_CHANGE; break;
                case 0x02: *out_cmd = MSG_EVT_AUDIO_MUTE_CHANGE; break;
                case 0x03: *out_cmd = MSG_EVT_AUDIO_SOURCE_CHANGE; break;
                default: *out_cmd = MSG_CMD_UNDEFINED; break;
            }

            MSG_DEBUG("Converted audio event: volume=%u, mute=%u, source=%u, cmd=0x%x",
                     out_payload->event_data.audio_event.volume_level,
                     out_payload->event_data.audio_event.mute_state,
                     out_payload->event_data.audio_event.audio_source, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_POWER: {
            out_payload->msg_type = RAW_MSG_TYPE_POWER;

            // 解析电源消息
            out_payload->event_data.power_event.power_state = (raw_msg_data & 0xFF);
            out_payload->event_data.power_event.battery_level = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.power_event.charging_state = ((raw_msg_data >> 16) & 0xFF);
            out_payload->event_data.power_event.timestamp = out_payload->timestamp;

            // 根据电源事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_POWER_ACC_ON; break;
                case 0x02: *out_cmd = MSG_EVT_POWER_ACC_OFF; break;
                case 0x03: *out_cmd = MSG_EVT_POWER_LOW_BATTERY; break;
                case 0x04: *out_cmd = MSG_EVT_POWER_CHARGING; break;
                default: *out_cmd = MSG_CMD_UNDEFINED; break;
            }

            MSG_DEBUG("Converted power event: state=%u, battery=%u, charging=%u, cmd=0x%x",
                     out_payload->event_data.power_event.power_state,
                     out_payload->event_data.power_event.battery_level,
                     out_payload->event_data.power_event.charging_state, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_USB: {
            out_payload->msg_type = RAW_MSG_TYPE_USB;

            // 解析USB消息
            out_payload->event_data.usb_event.device_type = (raw_msg_data & 0xFF);
            out_payload->event_data.usb_event.device_id = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.usb_event.connection_state = ((raw_msg_data >> 16) & 0xFF);
            out_payload->event_data.usb_event.timestamp = out_payload->timestamp;
            snprintf(out_payload->event_data.usb_event.device_name,
                    sizeof(out_payload->event_data.usb_event.device_name),
                    "USB_Device_%u", out_payload->event_data.usb_event.device_id);

            // 根据USB事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_USB_CONNECTED; break;
                case 0x02: *out_cmd = MSG_EVT_USB_DISCONNECTED; break;
                default: *out_cmd = MSG_CMD_UNDEFINED; break;
            }

            MSG_DEBUG("Converted USB event: type=%u, id=%u, state=%u, cmd=0x%x",
                     out_payload->event_data.usb_event.device_type,
                     out_payload->event_data.usb_event.device_id,
                     out_payload->event_data.usb_event.connection_state, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_VEHICLE: {
            out_payload->msg_type = RAW_MSG_TYPE_VEHICLE;

            // 解析车辆消息
            out_payload->event_data.vehicle_event.gear_state = (raw_msg_data & 0xFF);
            out_payload->event_data.vehicle_event.handbrake_state = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.vehicle_event.speed = ((raw_msg_data >> 16) & 0xFFFF);
            out_payload->event_data.vehicle_event.timestamp = out_payload->timestamp;

            // 根据车辆事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_REVERSE_GEAR_ON; break;
                case 0x02: *out_cmd = MSG_EVT_REVERSE_GEAR_OFF; break;
                case 0x03: *out_cmd = MSG_EVT_HANDBRAKE_ON; break;
                case 0x04: *out_cmd = MSG_EVT_HANDBRAKE_OFF; break;
                default: *out_cmd = MSG_CMD_UNDEFINED; break;
            }

            MSG_DEBUG("Converted vehicle event: gear=%u, handbrake=%u, speed=%u, cmd=0x%x",
                     out_payload->event_data.vehicle_event.gear_state,
                     out_payload->event_data.vehicle_event.handbrake_state,
                     out_payload->event_data.vehicle_event.speed, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_NETWORK: {
            out_payload->msg_type = RAW_MSG_TYPE_NETWORK;

            // 解析网络消息
            out_payload->event_data.network_event.network_type = (raw_msg_data & 0xFF);
            out_payload->event_data.network_event.connection_state = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.network_event.signal_strength = ((raw_msg_data >> 16) & 0xFF);
            out_payload->event_data.network_event.timestamp = out_payload->timestamp;
            snprintf(out_payload->event_data.network_event.network_name,
                    sizeof(out_payload->event_data.network_event.network_name),
                    "Network_%u", out_payload->event_data.network_event.network_type);

            // 根据网络事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_WIFI_CONNECTED; break;
                case 0x02: *out_cmd = MSG_EVT_WIFI_DISCONNECTED; break;
                case 0x03: *out_cmd = MSG_EVT_BLUETOOTH_PAIRED; break;
                case 0x04: *out_cmd = MSG_EVT_BLUETOOTH_DISCONNECTED; break;
                default: *out_cmd = MSG_EVT_NETWORK_ERROR; break;
            }

            MSG_DEBUG("Converted network event: type=%u, state=%u, signal=%u, cmd=0x%x",
                     out_payload->event_data.network_event.network_type,
                     out_payload->event_data.network_event.connection_state,
                     out_payload->event_data.network_event.signal_strength, *out_cmd);
            break;
        }

        case RAW_MSG_TYPE_STORAGE: {
            out_payload->msg_type = RAW_MSG_TYPE_STORAGE;

            // 解析存储消息
            out_payload->event_data.storage_event.storage_type = (raw_msg_data & 0xFF);
            out_payload->event_data.storage_event.storage_state = ((raw_msg_data >> 8) & 0xFF);
            out_payload->event_data.storage_event.total_space = ((raw_msg_data >> 16) & 0xFFFF);
            out_payload->event_data.storage_event.free_space = out_payload->event_data.storage_event.total_space / 2; // 示例
            out_payload->event_data.storage_event.timestamp = out_payload->timestamp;
            snprintf(out_payload->event_data.storage_event.storage_path,
                    sizeof(out_payload->event_data.storage_event.storage_path),
                    "/mnt/storage_%u", out_payload->event_data.storage_event.storage_type);

            // 根据存储事件类型确定命令
            switch (raw_msg_id) {
                case 0x01: *out_cmd = MSG_EVT_STORAGE_MOUNTED; break;
                case 0x02: *out_cmd = MSG_EVT_STORAGE_UNMOUNTED; break;
                case 0x03: *out_cmd = MSG_EVT_STORAGE_FULL; break;
                default: *out_cmd = MSG_EVT_STORAGE_ERROR; break;
            }

            MSG_DEBUG("Converted storage event: type=%u, state=%u, total=%u, cmd=0x%x",
                     out_payload->event_data.storage_event.storage_type,
                     out_payload->event_data.storage_event.storage_state,
                     out_payload->event_data.storage_event.total_space, *out_cmd);
            break;
        }

        default: {
            MSG_ERROR("Unknown raw message type: %u", raw_msg_type);
            g_msg_ctx.conversion_errors++;
            return -1;
        }
    }

    if (*out_cmd == MSG_CMD_UNDEFINED) {
        MSG_ERROR("Failed to convert message to valid command");
        g_msg_ctx.conversion_errors++;
        return -1;
    }

    g_msg_ctx.total_messages_converted++;
    return 0;
}

// --- 消息分发函数 ---
static void msg_dispatch_converted_message(msg_message_cmd_t cmd, msg_payload_service_msg_t* payload) {
    if (!payload) {
        MSG_ERROR("Cannot dispatch message with null payload");
        return;
    }

    MSG_DEBUG("Dispatching converted message: cmd=0x%x, type=%u", cmd, payload->msg_type);

    // 根据消息类型决定分发目标
    bus_router_address_t target_address = ADDRESS_NONE;

    switch (payload->msg_type) {
        case RAW_MSG_TYPE_SYSTEM:
            // 系统消息发送给系统服务
            target_address = ADDRESS_SYSTEM_SERVICE;
            break;

        case RAW_MSG_TYPE_AUDIO:
            // 音频消息可能需要发送给多个服务
            target_address = ADDRESS_UI | ADDRESS_RADIO_SERVICE;
            break;

        case RAW_MSG_TYPE_POWER:
            // 电源消息广播给所有服务
            target_address = ADDRESS_BROADCAST;
            break;

        case RAW_MSG_TYPE_USB:
        case RAW_MSG_TYPE_VEHICLE:
            // USB和车辆消息发送给系统服务和UI
            target_address = ADDRESS_SYSTEM_SERVICE | ADDRESS_UI;
            break;

        case RAW_MSG_TYPE_NETWORK:
            // 网络消息发送给系统服务和UI
            target_address = ADDRESS_SYSTEM_SERVICE | ADDRESS_UI;
            break;

        case RAW_MSG_TYPE_STORAGE:
            // 存储消息发送给系统服务和UI
            target_address = ADDRESS_SYSTEM_SERVICE | ADDRESS_UI;
            break;

        default:
            MSG_ERROR("Unknown message type for dispatch: %u", payload->msg_type);
            return;
    }

    // 创建payload副本用于发送
    msg_payload_service_msg_t* payload_copy = service_msg_payload_copy(payload);
    if (!payload_copy) {
        MSG_ERROR("Failed to create payload copy for dispatch");
        return;
    }

    // 发送消息
    int result = bus_post_message(SRC_MSG_SERVICE, target_address, MSG_PRIO_NORMAL,
                                 cmd, payload_copy, SRC_MSG_SERVICE, NULL);

    if (result == 0) {
        g_msg_ctx.total_messages_dispatched++;
        MSG_DEBUG("Message dispatched successfully to address 0x%llx", target_address);
    } else {
        MSG_ERROR("Failed to dispatch message, error: %d", result);
        service_msg_payload_free(payload_copy);
    }
}

// --- 外部接口：消息转换和分发 ---
int service_msg_convert_and_dispatch(uint32_t raw_msg_type, uint32_t raw_msg_id, uint32_t raw_msg_data) {
    if (!g_msg_ctx.service_ready) {
        MSG_ERROR("Message service not ready");
        return -1;
    }

    g_msg_ctx.total_messages_received++;

    msg_message_cmd_t converted_cmd;
    msg_payload_service_msg_t* payload = service_msg_payload_alloc();
    if (!payload) {
        MSG_ERROR("Failed to alloc payload for conversion");
        g_msg_ctx.conversion_errors++;
        return -1;
    }

    // 转换原始消息
    if (msg_convert_raw_message(raw_msg_type, raw_msg_id, raw_msg_data, &converted_cmd, payload) != 0) {
        MSG_ERROR("Failed to convert raw message");
        return -1;
    }

    // 分发转换后的消息
    msg_dispatch_converted_message(converted_cmd, payload);

     // 【关键】因为 dispatch 函数处理的是副本，我们必须在这里释放我们为转换而分配的原始 payload
    service_msg_payload_free(payload);

    return 0;
}

// --- 原始消息钩子注册/注销 ---
int service_msg_register_raw_message_hook(void) {
    MSG_INFO("Registering raw message hook...");

    // 这里需要调用 msg_emit.c 中的注册函数
    // 由于我们没有直接访问 msg_emit.c 的接口，这里提供一个框架
    // 实际实现需要根据具体的 msg_emit.c 接口来调整

    
    __memit_hook_t hook;
    hook.init_hook = msg_raw_system_hook;

    if (msg_emit_register_hook(&hook, GUI_MSG_HOOK_TYPE_INIT) != 0) {
        MSG_ERROR("Failed to register message hook");
        return -1;
    }
    

    g_msg_ctx.hook_registered = true;
    MSG_INFO("Raw message hook registered successfully");
    return 0;
}

int service_msg_unregister_raw_message_hook(void) {
    if (!g_msg_ctx.hook_registered) {
        return 0;
    }

    MSG_INFO("Unregistering raw message hook...");

    // 这里需要调用 msg_emit.c 中的注销函数
    
    if (msg_emit_unregister_hook(GUI_MSG_HOOK_TYPE_INIT) != 0) {
        MSG_ERROR("Failed to unregister message hook");
        return -1;
    }
    

    g_msg_ctx.hook_registered = false;
    MSG_INFO("Raw message hook unregistered successfully");
    return 0;
}

// ================================
// 定时器管理函数实现
// ================================

static void msg_timer_start(msg_internal_timer_type_t timer_type, uint32_t duration_ms,
                           timer_type_t type, unsigned int target_command, void* user_data) {
    // 在启动新定时器前，先确保同类型的旧定时器已被停止
    msg_timer_stop(timer_type);

    uint32_t new_id = timer_start(
        duration_ms,
        type,
        ADDRESS_MSG_SERVICE,
        target_command,
        user_data,
        user_data ? SRC_MSG_SERVICE : SRC_UNDEFINED
    );

    if (new_id != TIMER_SERVICE_INVALID_ID) {
        // 成功启动，将ID保存到我们的管理数组中
        for (int i = 0; i < MSG_TIMER_TYPE_COUNT; i++) {
            if (g_msg_ctx.active_timers[i].type == MSG_TIMER_NONE) {
                g_msg_ctx.active_timers[i].type = timer_type;
                g_msg_ctx.active_timers[i].timer_id = new_id;
                MSG_DEBUG("Internal timer started: type=%d, id=%u", timer_type, new_id);
                return;
            }
        }
        // 如果数组满了（理论上不应该发生），停止刚刚创建的定时器
        MSG_ERROR("Active timers array is full! Stopping orphaned timer %u", new_id);
        timer_stop(new_id);
    } else {
        MSG_ERROR("Failed to start internal timer: type=%d", timer_type);
    }
}

static void msg_timer_stop(msg_internal_timer_type_t timer_type) {
    for (int i = 0; i < MSG_TIMER_TYPE_COUNT; i++) {
        if (g_msg_ctx.active_timers[i].type == timer_type) {
            if (g_msg_ctx.active_timers[i].timer_id != TIMER_SERVICE_INVALID_ID) {
                timer_stop(g_msg_ctx.active_timers[i].timer_id);
                MSG_DEBUG("Internal timer stopped: type=%d, id=%u", timer_type, g_msg_ctx.active_timers[i].timer_id);
            }
            // 从管理数组中移除
            g_msg_ctx.active_timers[i].type = MSG_TIMER_NONE;
            g_msg_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
            return;
        }
    }
    MSG_DEBUG("Timer type %d not found in active timers", timer_type);
}

static void msg_timer_stop_all(void) {
    MSG_INFO("Stopping all message timers...");
    for (int i = 0; i < MSG_TIMER_TYPE_COUNT; i++) {
        if (g_msg_ctx.active_timers[i].type != MSG_TIMER_NONE) {
            msg_timer_stop(g_msg_ctx.active_timers[i].type);
        }
    }
    MSG_INFO("All message timers stopped");
}

// ================================
// 外部便利函数实现
// ================================

void service_msg_start_system_check_timer(uint32_t interval_ms) {
    MSG_INFO("Starting system check timer: %u ms interval", interval_ms);
    msg_timer_start(MSG_TIMER_SYSTEM_CHECK, interval_ms, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_SYSTEM_CHECK, NULL);
}

void service_msg_start_driver_watchdog_timer(uint32_t timeout_ms) {
    MSG_INFO("Starting driver watchdog timer: %u ms timeout", timeout_ms);
    msg_timer_start(MSG_TIMER_DRIVER_WATCHDOG, timeout_ms, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_DRIVER_WATCHDOG, NULL);
}

void service_msg_start_vehicle_status_check_timer(uint32_t interval_ms) {
    MSG_INFO("Starting vehicle status check timer: %u ms interval", interval_ms);
    msg_timer_start(MSG_TIMER_VEHICLE_STATUS_CHECK, interval_ms, TIMER_TYPE_PERIODIC,
                   MSG_CMD_INTERNAL_VEHICLE_STATUS_CHECK, NULL);
}

void service_msg_stop_all_timers(void) {
    MSG_INFO("Stopping all message timers (external call)");
    msg_timer_stop_all();
}


// --- 系统消息钩子 ---
int32_t msg_raw_system_hook(void* msg) {
    if (!msg) {
        MSG_HOOK_ERROR("Received null system message");
        return -1;
    }

    if(!g_msg_ctx.service_ready) // 消息服务未就绪，直接返回
    {
        MSG_HOOK_ERROR("Message service not ready");
        return -1;
    }
    __gui_msg_t* gui_msg = (__gui_msg_t*)msg;
    
    MSG_HOOK_DEBUG("System hook: id=0x%x, data=0x%x", gui_msg->id, gui_msg->dwAddData2);
    
    // 根据系统消息类型进行分类
    uint32_t raw_msg_type;
    
    if (gui_msg->id >= GUI_MSG_UI_SYS_SOURCE && gui_msg->id <= GUI_MSG_UI_SYS_HOME_EXIT) {
        raw_msg_type = RAW_MSG_TYPE_SYSTEM;
    } else if (gui_msg->id >= GUI_MSG_UI_SYS_VOLUME && gui_msg->id <= GUI_MSG_UI_SYS_MUTE) {
        raw_msg_type = RAW_MSG_TYPE_AUDIO;
    } else {
        raw_msg_type = RAW_MSG_TYPE_SYSTEM;
    }
    
    uint32_t raw_msg_id = gui_msg->id;
    uint32_t raw_msg_data = gui_msg->dwAddData2;
    
    return service_msg_convert_and_dispatch(raw_msg_type, raw_msg_id, raw_msg_data);

}



// --- 测试函数：模拟原始消息 ---
void service_msg_test_simulate_raw_messages(void) {
    MSG_HOOK_DEBUG("Simulating raw messages for testing...");
    // 模拟系统消息
    msg_raw_system_hook(NULL);
}

// --- 消息统计和调试函数 ---
void service_msg_dump_statistics(void) {
    // 这个函数可以用来输出消息转换的统计信息
    // 需要访问 service_msg.c 中的统计数据
    MSG_HOOK_DEBUG("Message conversion statistics:");
    MSG_HOOK_DEBUG("  - This function needs to be implemented with proper access to stats");
}


