#include "service_mcu.h"
#include "bus_router.h"
#include <kapi.h>
#include "mcu/mcu_hal.h"
#include "mod_desktop/desktop.h"
#include <radio/radio_hal.h>


// --- 调试功能相关定义 ---
static int g_mcu_service_debug = 1;  // 调试开关，默认关闭
static int g_mcu_service_info = 1;   // 信息开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define MCU_DEBUG(fmt, ...) \
    do { \
        if (g_mcu_service_debug) { \
            printf("[McuService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define MCU_INFO(fmt, ...) \
    do { \
        if (g_mcu_service_info) { \
            printf("[McuService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define MCU_ERROR(fmt, ...) \
    printf("[McuService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

// --- 内部定义 ---
#define MCU_QUEUE_SIZE 128
#define MCU_THREAD_STACK_SIZE (1024 * 8)
#define MCU_SERVICE_NAME "ServiceMcu"
#define MCU_DEVICE_PATH "/dev/mcu/"

// 【删除】MCU命令定义 - 直接使用 dsk_mcu.c 和 mod_mcu.h 中的定义
// 【删除】IO选择定义 - 直接使用 dsk_mcu.c 和 mod_mcu.h 中的定义

// 【新增】定义一个结构体来保存单个定时器的信息
typedef struct {
    mcu_internal_timer_type_t type;     // 定时器的用途
    uint32_t                  timer_id; // 从 service_timer 返回的ID
} mcu_active_timer_info_t;

// 【新增】MCU 服务上下文结构体
typedef struct {
    // 使用一个固定大小的数组来管理所有活动定时器
    mcu_active_timer_info_t active_timers[MCU_TIMER_TYPE_COUNT];
    
    // MCU状态
    mcu_state_t current_state;
    mcu_state_payload_t mcu_data;
    
    // 统计信息
    uint32_t total_commands_sent;
    uint32_t total_responses_received;
    uint32_t command_errors;
    uint32_t sequence_counter;
    
    // 服务状态
    bool service_ready;
    bool device_opened;
} mcu_service_context_t;

// --- 全局变量 ---
static void* g_mcu_queue_handle = NULL;
static int g_mcu_thread_handle = 0;
static void* g_mcu_mutex = NULL;
static void* g_mcu_semaphore = NULL;
static mcu_service_context_t g_mcu_ctx;

// Payload 管理
//static mcu_state_payload_t* service_mcu_payload = NULL;
static int service_mcu_payload_use_count = 0;

// --- 内部函数声明 ---
static void mcu_thread_entry(void* p_arg);
static void mcu_event_handler(app_message_t* msg);
static void mcu_timer_start(mcu_internal_timer_type_t timer_type, uint32_t duration_ms, 
                           timer_type_t type, unsigned int target_command, void* user_data);
static void mcu_timer_stop(mcu_internal_timer_type_t timer_type);
static void mcu_timer_stop_all(void);
static void mcu_handle_internal_timer_timeout(unsigned int command);

// 【删除】MCU设备操作函数 - 直接使用 dsk_mcu.c 中的函数

// --- 外部控制调试开关的函数 ---
void service_mcu_set_debug(int enable) {
    g_mcu_service_debug = enable;
    MCU_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void service_mcu_set_info(int enable) {
    g_mcu_service_info = enable;
    MCU_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

// --- Payload 管理函数 ---
mcu_state_payload_t* service_mcu_payload_alloc(void) {
    unsigned char err;
    if (g_mcu_semaphore)
        infotainment_sem_wait(g_mcu_semaphore, 0, &err);
    if (service_mcu_payload_use_count >= MCU_QUEUE_SIZE) {
        if (g_mcu_semaphore)
            infotainment_sem_post(g_mcu_semaphore);
        MCU_ERROR("Payload allocation limit reached");
        return NULL;
    }
    
    mcu_state_payload_t* service_mcu_payload = (mcu_state_payload_t*)infotainment_malloc(sizeof(mcu_state_payload_t));
    if (!service_mcu_payload) {
        MCU_ERROR("Failed to allocate payload memory");
        if (g_mcu_semaphore)
            infotainment_sem_post(g_mcu_semaphore);
        return NULL;
    }
    
    memset(service_mcu_payload, 0, sizeof(mcu_state_payload_t));
    service_mcu_payload_use_count++;
    service_mcu_payload->sequence_id = ++g_mcu_ctx.sequence_counter;
    service_mcu_payload->timestamp = infotainment_get_ticks();
    
    if (g_mcu_semaphore)
        infotainment_sem_post(g_mcu_semaphore);
    
    return service_mcu_payload;
}

void service_mcu_payload_free(mcu_state_payload_t* payload) {
    if (!payload) return;
    
    unsigned char err;
    if (g_mcu_semaphore)
        infotainment_sem_wait(g_mcu_semaphore, 0, &err);
    
    infotainment_free(payload);
    service_mcu_payload_use_count--;
    
    if (g_mcu_semaphore)
        infotainment_sem_post(g_mcu_semaphore);
    MCU_DEBUG("Payload memory freed");
}

mcu_state_payload_t* service_mcu_payload_copy(const mcu_state_payload_t* src) {
    if (!src) return NULL;
    
    mcu_state_payload_t* copy = service_mcu_payload_alloc();
    if (copy) {
        memcpy(copy, src, sizeof(mcu_state_payload_t));
        copy->sequence_id = ++g_mcu_ctx.sequence_counter; // 新的序列号
        copy->timestamp = infotainment_get_ticks();       // 新的时间戳
    }
    return copy;
}

// 【删除】MCU设备操作函数 - 直接使用 dsk_mcu.c 中的函数

// --- 主要接口函数 ---
void service_mcu_init(void) {
    unsigned char err = 0;
    MCU_INFO("Initializing MCU service...");

    // 初始化服务上下文
    memset(&g_mcu_ctx, 0, sizeof(mcu_service_context_t));
    for (int i = 0; i < MCU_TIMER_TYPE_COUNT; i++) {
        g_mcu_ctx.active_timers[i].type = MCU_TIMER_NONE;
        g_mcu_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
    }
    g_mcu_ctx.current_state = MCU_STATE_INITIALIZING;

    // 创建信号量
    g_mcu_semaphore = infotainment_sem_create(1);
    if (!g_mcu_semaphore) {
        MCU_ERROR("Failed to create semaphore");
        return;
    }

    // 创建互斥锁
    g_mcu_mutex = infotainment_mutex_create(0, &err);
    if (!g_mcu_mutex || err != 0) {
        MCU_ERROR("Failed to create mutex, error: %d", err);
        infotainment_sem_delete(g_mcu_semaphore, &err);
        g_mcu_semaphore = NULL;
        return;
    }

    mcu_hal_init_para();
    // 创建线程
    g_mcu_thread_handle = infotainment_thread_create(
        mcu_thread_entry, NULL, TASK_PROI_LEVEL2,
        MCU_THREAD_STACK_SIZE, MCU_SERVICE_NAME
    );
    if (g_mcu_thread_handle <= 0) {
        MCU_ERROR("Failed to create thread");
        infotainment_mutex_delete(g_mcu_mutex, &err);
        infotainment_sem_delete(g_mcu_semaphore, &err);
        g_mcu_mutex = NULL;
        g_mcu_semaphore = NULL;
        return;
    }

    MCU_INFO("MCU service initialized successfully");
}

void service_mcu_exit(void) {
    unsigned char err = 0;
    MCU_INFO("Exiting MCU service...");

    if (!g_mcu_ctx.service_ready) {
        return;
    }

    // 停止所有定时器
    mcu_timer_stop_all();

    // 【修改】MCU设备由 dsk_mcu.c 管理，这里只需要标记为未准备
    g_mcu_ctx.device_opened = false;

    // 注销服务
    bus_router_unregister_service(SRC_MCU_SERVICE);

    // 请求线程退出
    if (g_mcu_thread_handle > 0) {
        MCU_DEBUG("Requesting MCU thread deletion...");
        infotainment_thread_delete_req(g_mcu_thread_handle);
        g_mcu_thread_handle = 0;
    }

    // 清理资源
    if (g_mcu_queue_handle) {
        infotainment_queue_delete(g_mcu_queue_handle, &err);
        g_mcu_queue_handle = NULL;
    }

    if (g_mcu_mutex) {
        infotainment_mutex_delete(g_mcu_mutex, &err);
        g_mcu_mutex = NULL;
    }

    if (g_mcu_semaphore) {
        infotainment_sem_delete(g_mcu_semaphore, &err);
        g_mcu_semaphore = NULL;
    }

    // 重置上下文
    g_mcu_ctx.current_state = MCU_STATE_SHUTDOWN;
    memset(&g_mcu_ctx, 0, sizeof(mcu_service_context_t));

    MCU_INFO("MCU service cleanup complete");
}

// --- 线程入口函数 ---
static void mcu_thread_entry(void* p_arg) {
    unsigned char err = 0;
    MCU_INFO("MCU service thread started");

    // 1. 创建消息队列
    g_mcu_queue_handle = infotainment_queue_create(MCU_QUEUE_SIZE);
    if (!g_mcu_queue_handle) {
        MCU_ERROR("Failed to create message queue");
        infotainment_thread_delete(g_mcu_thread_handle);
        return;
    }
    MCU_INFO("MCU message queue created with handle %p", g_mcu_queue_handle);

    // 2. 注册服务到消息总线路由器
    if (bus_router_register_service(SRC_MCU_SERVICE, ADDRESS_MCU_SERVICE, g_mcu_queue_handle, "MCU service",service_mcu_payload_free,service_mcu_payload_copy) < 0) {
        MCU_ERROR("Failed to register service with bus router");
        infotainment_queue_delete(g_mcu_queue_handle, &err);
        infotainment_thread_delete(g_mcu_thread_handle);
        return;
    }

    // 3. 【修改】MCU设备由 dsk_mcu.c 管理，这里只需要标记为已准备
    g_mcu_ctx.device_opened = true;
    MCU_INFO("MCU service ready to use dsk_mcu functions");

    // 4. 启动内部定时器
    MCU_INFO("Starting MCU service timers...");
    #if 0
    mcu_timer_start(MCU_TIMER_POWER_CHECK, 5000, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_POWER_CHECK, NULL);
    mcu_timer_start(MCU_TIMER_DATA_SAVE, 30000, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_DATA_SAVE, NULL);
    mcu_timer_start(MCU_TIMER_IO_MONITOR, 1000, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_IO_MONITOR, NULL);
    mcu_timer_start(MCU_TIMER_JUMPER_DETECT, 10000, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_JUMPER_DETECT, NULL);
    mcu_timer_start(MCU_TIMER_HEARTBEAT, 2000, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_HEARTBEAT, NULL);

    #endif

    g_mcu_ctx.current_state = MCU_STATE_RUNNING;
    g_mcu_ctx.service_ready = true;
    MCU_INFO("MCU service ready");

    // 5. 主消息循环
    while (1) {
        // 检查是否有线程删除请求
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            MCU_DEBUG("MCU service thread delete request received");
            goto exit;
        }

        // 获取消息
        app_message_t* received_msg = (app_message_t*)infotainment_queue_pend(g_mcu_queue_handle, 0, &err);
        if (received_msg && err == OS_NO_ERR) {
            MCU_DEBUG("收到消息 (来源:0x%x, 命令:0x%x)", received_msg->source, received_msg->command);
            mcu_event_handler(received_msg);

            // 释放消息内存
            if (received_msg->payload) {
                free_ref_counted_payload((ref_counted_payload_t *)received_msg->payload);
            }
            infotainment_free(received_msg);
            received_msg = NULL;
            MCU_DEBUG("Message processed and memory freed");
        }
    }

exit:
    // 清理线程资源
    MCU_DEBUG("MCU service thread cleanup started");

    // 清空消息队列中的所有消息，防止内存泄漏
    if (g_mcu_queue_handle) {
        void* msg = NULL;
        unsigned char queue_err = 0;

        while ((msg = infotainment_queue_pend(g_mcu_queue_handle, 0, &queue_err)) && queue_err == EPDK_OK) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg && app_msg->payload) {
                infotainment_free(app_msg->payload);
            }
            if (app_msg) {
                infotainment_free(app_msg);
            }
            MCU_DEBUG("Freed queued message during cleanup");
        }
    }

    MCU_DEBUG("MCU service thread cleanup completed");

    // 删除线程自身
    infotainment_thread_delete(g_mcu_thread_handle);
}

// --- 消息事件处理函数 ---
static void mcu_event_handler(app_message_t* msg) {
    if (!msg) {
        MCU_ERROR("Received invalid message");
        return;
    }

    MCU_DEBUG("Processing message from source 0x%x, command 0x%x", msg->source, msg->command);

    // 1. 处理全局命令
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
            if (msg->command >= MCU_CMD_INTERNAL_POWER_CHECK &&
                msg->command <= MCU_CMD_INTERNAL_BACKUP_CHECK) {
                mcu_handle_internal_timer_timeout(msg->command);
            }
            break;
        }

        case SRC_MCU_SERVICE: {
            // 处理来自自己的内部消息（定时器超时等）
            if (msg->command >= MCU_CMD_INTERNAL_POWER_CHECK &&
                msg->command <= MCU_CMD_INTERNAL_BACKUP_CHECK) {
                mcu_handle_internal_timer_timeout(msg->command);
            }
            break;
        }

        case SRC_SYSTEM_SERVICE:
        case SRC_UI:
            // 处理来自系统服务和UI的消息
            break;
        case SRC_RADIO_SERVICE: {
            // 处理来自其他服务的MCU命令
           
            break;
        }

        default: {
            MCU_ERROR("Received message from unknown source 0x%x", msg->source);
            break;
        }
    }
}

// --- 内部定时器超时处理函数 ---
static void mcu_handle_internal_timer_timeout(unsigned int command) {
    MCU_DEBUG("Handling internal timer timeout: command=0x%x", command);

    switch (command) {
        case MCU_CMD_INTERNAL_POWER_CHECK:
            MCU_DEBUG("Performing power status check");
            // 检查电源状态
            // 可以调用 service_mcu_get_power_status() 等函数
            break;

        case MCU_CMD_INTERNAL_DATA_SAVE:
            MCU_DEBUG("Performing periodic data save");
            // 【修改】执行定期数据保存 - 使用 dsk_mcu.c 中的函数
            #ifdef MCU_FLASH_SAVE_ENABLE
            service_mcu_flush_flash_memory(); // 刷新flash内存
            #endif
            service_mcu_save_data(0); // 保存所有类型的数据
            break;

        case MCU_CMD_INTERNAL_IO_MONITOR:
            MCU_DEBUG("Monitoring IO status");
            // 监控IO口状态
            break;

        case MCU_CMD_INTERNAL_JUMPER_DETECT:
            MCU_DEBUG("Detecting jumper status");
            // 检测跳线状态
            service_mcu_detect_jumper();
            break;

        case MCU_CMD_INTERNAL_WATCHDOG:
            MCU_DEBUG("MCU watchdog check");
            // MCU看门狗检查
            break;

        case MCU_CMD_INTERNAL_HEARTBEAT:
            MCU_DEBUG("MCU heartbeat");
            // 发送心跳信号
            MCU_INFO("MCU stats: commands=%u, responses=%u, errors=%u",
                    g_mcu_ctx.total_commands_sent,
                    g_mcu_ctx.total_responses_received,
                    g_mcu_ctx.command_errors);
            break;

        case MCU_CMD_INTERNAL_BACKUP_CHECK:
            MCU_DEBUG("Backup check");
            // 备份检查
            break;

        default:
            MCU_ERROR("Unknown internal timer command: 0x%x", command);
            break;
    }
}

// ================================
// 电源管理接口实现
// ================================

int service_mcu_power_on(void) {
    MCU_INFO("MCU power on requested");
    // 这里可以添加具体的上电逻辑
    // 通常上电是由硬件控制的，软件只需要初始化
    g_mcu_ctx.current_state = MCU_STATE_RUNNING;
    return 0;
}

int service_mcu_power_off(void) {
    MCU_INFO("MCU power off requested");

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_power_off();
    if (ret == 0) {
        g_mcu_ctx.current_state = MCU_STATE_SHUTDOWN;
        g_mcu_ctx.total_commands_sent++;
        MCU_INFO("Power off command sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send power off command");
    }

    return ret;
}

int service_mcu_power_reset(void) {
    MCU_INFO("MCU reset requested");

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_reset();
    if (ret == 0) {
        g_mcu_ctx.current_state = MCU_STATE_INITIALIZING;
        g_mcu_ctx.total_commands_sent++;
        MCU_INFO("Reset command sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send reset command");
    }

    return ret;
}

int service_mcu_power_restart(uint8_t flag) {
    MCU_INFO("MCU restart requested, flag: %d", flag);

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_restart(flag);
    if (ret == 0) {
        g_mcu_ctx.current_state = MCU_STATE_INITIALIZING;
        g_mcu_ctx.total_commands_sent++;
        MCU_INFO("Restart command sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send restart command");
    }

    return ret;
}

int service_mcu_get_power_status(mcu_power_status_t* status) {
    if (!status) {
        MCU_ERROR("Invalid power status pointer");
        return -1;
    }

    // 这里应该从MCU读取实际的电源状态
    // 目前提供模拟数据
    status->power_state = (g_mcu_ctx.current_state == MCU_STATE_RUNNING) ? 1 : 0;
    status->voltage_level = 12000; // 12V
    status->current_level = 2000;  // 2A
    status->temperature = 45;      // 45°C
    status->timestamp = infotainment_get_ticks();

    MCU_DEBUG("Power status: state=%u, voltage=%u, current=%u, temp=%u",
             status->power_state, status->voltage_level,
             status->current_level, status->temperature);

    return 0;
}

// ================================
// 数据存储接口实现
// ================================

int service_mcu_save_data(uint32_t data_type) {
    MCU_INFO("Saving data, type: %u", data_type);

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_memory(data_type);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Data save command sent successfully for type: %u", data_type);

        // 发送数据保存事件通知
        mcu_state_payload_t* payload = service_mcu_payload_alloc();
        if (payload) {
            payload->data.data_info.data_type = data_type;
            payload->data.data_info.data_size = 1024; // 示例大小
            payload->data.data_info.checksum = 0x12345678; // 示例校验和
            snprintf(payload->data.data_info.data_path, sizeof(payload->data.data_info.data_path),
                    "/mcu/data/type_%u.dat", data_type);
            payload->data.data_info.timestamp = infotainment_get_ticks();

            // 广播数据保存完成事件
            bus_post_message(SRC_MCU_SERVICE, ADDRESS_BROADCAST, MSG_PRIO_NORMAL,
                            MCU_EVT_DATA_SAVED, payload, SRC_MCU_SERVICE, NULL);
        }
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send data save command for type: %u", data_type);
    }

    return ret;
}

int service_mcu_load_data(uint32_t data_type, void* buffer, uint32_t size) {
    if (!buffer || size == 0) {
        MCU_ERROR("Invalid buffer or size for data load");
        return -1;
    }

    MCU_INFO("Loading data, type: %u, size: %u", data_type, size);

    // 【修改】直接调用 dsk_mcu.c 中的函数请求数据
    int ret = dsk_request_mcu_mamery_data((uint8_t)data_type);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Data load request sent successfully for type: %u", data_type);

        // 注意：实际的数据读取需要通过MCU的响应消息来获取
        // 这里只是发送了请求命令，数据会通过MCU的响应消息返回
        // 模拟数据加载（实际应该从MCU响应中获取）
        memset(buffer, 0xAA, size); // 填充示例数据
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send data load request for type: %u", data_type);
    }

    return ret;
}

int service_mcu_backup_data(void) {
    MCU_INFO("Backing up all data");

    // 实现数据备份逻辑
    // 可能需要保存多种类型的数据

    return 0;
}

int service_mcu_restore_data(void) {
    MCU_INFO("Restoring all data");

    // 实现数据恢复逻辑

    return 0;
}

// ================================
// IO口扩展接口实现
// ================================

int service_mcu_set_io(uint32_t io_mask, uint32_t io_value) {
    MCU_INFO("Setting IO: mask=0x%08X, value=0x%08X", io_mask, io_value);

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_io(io_mask, io_value);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("IO set command sent successfully");

        // 发送IO状态变化事件
        mcu_state_payload_t* payload = service_mcu_payload_alloc();
        if (payload) {
            payload->data.io_status.io_mask = io_mask;
            payload->data.io_status.io_value = io_value;
            payload->data.io_status.timestamp = infotainment_get_ticks();

            bus_post_message(SRC_MCU_SERVICE, ADDRESS_BROADCAST, MSG_PRIO_NORMAL,
                            MCU_EVT_IO_STATUS_CHANGED, payload, SRC_MCU_SERVICE, NULL);
        }
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send IO set command");
    }

    return ret;
}

int service_mcu_get_io(uint32_t io_mask, uint32_t* io_value) {
    if (!io_value) {
        MCU_ERROR("Invalid io_value pointer");
        return -1;
    }

    MCU_DEBUG("Getting IO status for mask: 0x%08X", io_mask);

    // 这里应该从MCU读取实际的IO状态
    // 目前提供模拟数据
    *io_value = io_mask & 0x0000FFFF; // 示例：低16位为高电平

    MCU_DEBUG("IO status: mask=0x%08X, value=0x%08X", io_mask, *io_value);
    return 0;
}

int service_mcu_toggle_io(uint32_t io_mask) {
    MCU_INFO("Toggling IO: mask=0x%08X", io_mask);

    // 先读取当前状态
    uint32_t current_value;
    if (service_mcu_get_io(io_mask, &current_value) != 0) {
        return -1;
    }

    // 翻转状态
    uint32_t new_value = current_value ^ io_mask;

    // 设置新状态
    return service_mcu_set_io(io_mask, new_value);
}

int service_mcu_config_io(uint32_t io_mask, uint32_t config) {
    MCU_INFO("Configuring IO: mask=0x%08X, config=0x%08X", io_mask, config);

    // 这里应该实现IO配置逻辑
    // 比如设置IO方向、上拉下拉等

    return 0;
}

// ================================
// 跳线检测接口实现
// ================================

int service_mcu_detect_jumper(void) {
    MCU_DEBUG("Detecting jumper status");

    // 这里应该实现具体的跳线检测逻辑
    // 通常通过读取特定的IO口状态来判断跳线状态

    // 模拟跳线检测
    static uint32_t last_jumper_value = 0;
    uint32_t current_jumper_value = 0x0F; // 示例跳线值

    if (current_jumper_value != last_jumper_value) {
        MCU_INFO("Jumper status changed: 0x%08X -> 0x%08X", last_jumper_value, current_jumper_value);

        // 发送跳线状态变化事件
        mcu_state_payload_t* payload = service_mcu_payload_alloc();
        if (payload) {
            payload->data.jumper_status.jumper_mask = 0xFFFFFFFF;
            payload->data.jumper_status.jumper_value = current_jumper_value;
            payload->data.jumper_status.jumper_config = 0;
            snprintf(payload->data.jumper_status.jumper_description,
                    sizeof(payload->data.jumper_status.jumper_description),
                    "Jumper config: 0x%08X", current_jumper_value);
            payload->data.jumper_status.timestamp = infotainment_get_ticks();

            bus_post_message(SRC_MCU_SERVICE, ADDRESS_BROADCAST, MSG_PRIO_NORMAL,
                            MCU_EVT_JUMPER_STATUS_CHANGED, payload, SRC_MCU_SERVICE, NULL);
        }

        last_jumper_value = current_jumper_value;
    }

    return 0;
}

int service_mcu_get_jumper_status(mcu_jumper_status_t* status) {
    if (!status) {
        MCU_ERROR("Invalid jumper status pointer");
        return -1;
    }

    // 这里应该读取实际的跳线状态
    status->jumper_mask = 0xFFFFFFFF;
    status->jumper_value = 0x0F; // 示例值
    status->jumper_config = 0;
    snprintf(status->jumper_description, sizeof(status->jumper_description),
            "Jumper config: 0x%08X", status->jumper_value);
    status->timestamp = infotainment_get_ticks();

    MCU_DEBUG("Jumper status: value=0x%08X", status->jumper_value);
    return 0;
}

// ================================
// MCU更新接口实现
// ================================

int service_mcu_update_begin(uint32_t data_length) {
    MCU_INFO("MCU update begin, data length: %u", data_length);

    g_mcu_ctx.current_state = MCU_STATE_UPDATING;

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_update_begin(data_length);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_INFO("MCU update begin command sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send MCU update begin command");
        g_mcu_ctx.current_state = MCU_STATE_ERROR;
    }

    return ret;
}

int service_mcu_update_data(uint16_t package_index, uint8_t data_length, uint8_t* data) {
    if (!data || data_length == 0) {
        MCU_ERROR("Invalid update data parameters");
        return -1;
    }

    MCU_DEBUG("MCU update data, package: %u, length: %u", package_index, data_length);

    // 【修改】直接调用 dsk_mcu.c 中的函数
    int ret = dsk_send_mcu_update_data(package_index, data_length, data);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("MCU update data package %u sent successfully", package_index);

        // 发送更新进度事件
        mcu_state_payload_t* payload = service_mcu_payload_alloc();
        if (payload) {
            payload->data.update_info.package_index = package_index;
            payload->data.update_info.current_size = package_index * data_length;
            payload->data.update_info.progress = (package_index * 100) / 1000; // 假设总共1000个包
            payload->data.update_info.timestamp = infotainment_get_ticks();

            bus_post_message(SRC_MCU_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL,
                            MCU_EVT_UPDATE_PROGRESS, payload, SRC_MCU_SERVICE, NULL);
        }
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send MCU update data package %u", package_index);
    }

    return ret;
}

int service_mcu_update_end(void) {
    MCU_INFO("MCU update end");

   
}

int service_mcu_update_verify(void) {
    MCU_INFO("MCU update verify");

    // 这里应该实现更新验证逻辑
    // 比如校验更新后的固件

    return 0;
}

// ================================
// 定时器管理函数实现
// ================================

static void mcu_timer_start(mcu_internal_timer_type_t timer_type, uint32_t duration_ms,
                           timer_type_t type, unsigned int target_command, void* user_data) {
    // 在启动新定时器前，先确保同类型的旧定时器已被停止
    mcu_timer_stop(timer_type);

    uint32_t new_id = timer_start(
        duration_ms,
        type,
        ADDRESS_MCU_SERVICE,
        target_command,
        user_data,
        user_data ? SRC_MCU_SERVICE : SRC_UNDEFINED
    );

    if (new_id != TIMER_SERVICE_INVALID_ID) {
        // 成功启动，将ID保存到我们的管理数组中
        for (int i = 0; i < MCU_TIMER_TYPE_COUNT; i++) {
            if (g_mcu_ctx.active_timers[i].type == MCU_TIMER_NONE) {
                g_mcu_ctx.active_timers[i].type = timer_type;
                g_mcu_ctx.active_timers[i].timer_id = new_id;
                MCU_DEBUG("Internal timer started: type=%d, id=%u", timer_type, new_id);
                return;
            }
        }
        // 如果数组满了（理论上不应该发生），停止刚刚创建的定时器
        MCU_ERROR("Active timers array is full! Stopping orphaned timer %u", new_id);
        timer_stop(new_id);
    } else {
        MCU_ERROR("Failed to start internal timer: type=%d", timer_type);
    }
}

static void mcu_timer_stop(mcu_internal_timer_type_t timer_type) {
    for (int i = 0; i < MCU_TIMER_TYPE_COUNT; i++) {
        if (g_mcu_ctx.active_timers[i].type == timer_type) {
            if (g_mcu_ctx.active_timers[i].timer_id != TIMER_SERVICE_INVALID_ID) {
                timer_stop(g_mcu_ctx.active_timers[i].timer_id);
                MCU_DEBUG("Internal timer stopped: type=%d, id=%u", timer_type, g_mcu_ctx.active_timers[i].timer_id);
            }
            // 从管理数组中移除
            g_mcu_ctx.active_timers[i].type = MCU_TIMER_NONE;
            g_mcu_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
            return;
        }
    }
    MCU_DEBUG("Timer type %d not found in active timers", timer_type);
}

static void mcu_timer_stop_all(void) {
    MCU_INFO("Stopping all MCU timers...");
    for (int i = 0; i < MCU_TIMER_TYPE_COUNT; i++) {
        if (g_mcu_ctx.active_timers[i].type != MCU_TIMER_NONE) {
            mcu_timer_stop(g_mcu_ctx.active_timers[i].type);
        }
    }
    MCU_INFO("All MCU timers stopped");
}

// ================================
// 外部便利函数实现
// ================================

void service_mcu_start_power_check_timer(uint32_t interval_ms) {
    MCU_INFO("Starting power check timer: %u ms interval", interval_ms);
    mcu_timer_start(MCU_TIMER_POWER_CHECK, interval_ms, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_POWER_CHECK, NULL);
}

void service_mcu_start_data_save_timer(uint32_t interval_ms) {
    MCU_INFO("Starting data save timer: %u ms interval", interval_ms);
    mcu_timer_start(MCU_TIMER_DATA_SAVE, interval_ms, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_DATA_SAVE, NULL);
}

void service_mcu_start_io_monitor_timer(uint32_t interval_ms) {
    MCU_INFO("Starting IO monitor timer: %u ms interval", interval_ms);
    mcu_timer_start(MCU_TIMER_IO_MONITOR, interval_ms, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_IO_MONITOR, NULL);
}

void service_mcu_start_jumper_detect_timer(uint32_t interval_ms) {
    MCU_INFO("Starting jumper detect timer: %u ms interval", interval_ms);
    mcu_timer_start(MCU_TIMER_JUMPER_DETECT, interval_ms, TIMER_TYPE_PERIODIC,
                   MCU_CMD_INTERNAL_JUMPER_DETECT, NULL);
}

void service_mcu_stop_all_timers(void) {
    MCU_INFO("Stopping all MCU timers (external call)");
    mcu_timer_stop_all();
}

// ================================
// 状态管理函数实现
// ================================

mcu_state_t service_mcu_get_state(void) {
    return g_mcu_ctx.current_state;
}

const char* service_mcu_state_to_string(mcu_state_t state) {
    switch (state) {
        case MCU_STATE_IDLE:         return "IDLE";
        case MCU_STATE_INITIALIZING: return "INITIALIZING";
        case MCU_STATE_RUNNING:      return "RUNNING";
        case MCU_STATE_POWER_SAVING: return "POWER_SAVING";
        case MCU_STATE_UPDATING:     return "UPDATING";
        case MCU_STATE_ERROR:        return "ERROR";
        case MCU_STATE_SHUTDOWN:     return "SHUTDOWN";
        default:                     return "UNKNOWN";
    }
}

// ================================
// 额外的便利函数 - 直接调用 dsk_mcu.c 中的函数
// ================================

int service_mcu_send_ready(void) {
    MCU_INFO("Sending MCU ready signal");
    int ret = dsk_send_mcu_ready();
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("MCU ready signal sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send MCU ready signal");
    }
    return ret;
}

int service_mcu_send_date_time(__awos_date_t date, __awos_time_t time) {
    MCU_INFO("Sending date/time to MCU: %04d-%02d-%02d %02d:%02d:%02d",
             date.year, date.month, date.day, time.hour, time.minute, time.second);
    int ret = dsk_send_mcu_date_time(date, time);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Date/time sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send date/time");
    }
    return ret;
}

int service_mcu_send_tuner_type(uint8_t tuner_type) {
    MCU_INFO("Sending tuner type to MCU: %u", tuner_type);
    int ret = dsk_send_tuner_type(tuner_type);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Tuner type sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send tuner type");
    }
    return ret;
}

int service_mcu_send_current_source(uint8_t source) {
    MCU_INFO("Sending current source to MCU: %u", source);
    int ret = dsk_send_mcu_current_source(source);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Current source sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send current source");
    }
    return ret;
}

int service_mcu_set_beep_on_off(uint8_t on_off) {
    MCU_INFO("Setting MCU beep: %s", on_off ? "ON" : "OFF");
    int ret = dsk_send_mcu_beep_on_off(on_off);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Beep setting sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send beep setting");
    }
    return ret;
}

int service_mcu_set_beep_setting(uint16_t time) {
    MCU_INFO("Setting MCU beep time: %u ms", time * 10);
    int ret = dsk_send_mcu_beep_setting(time);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Beep time setting sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send beep time setting");
    }
    return ret;
}

int service_mcu_send_power_off_status(uint8_t status) {
    MCU_INFO("Sending power off status to MCU: %u", status);
    int ret = dsk_send_mcu_power_off_status(status);
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Power off status sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send power off status");
    }
    return ret;
}

#ifdef MCU_FLASH_SAVE_ENABLE
int service_mcu_flush_flash_memory(void) {
    MCU_INFO("Flushing MCU flash memory");
    int ret = dsk_send_mcu_flush_flash_memory();
    if (ret == 0) {
        g_mcu_ctx.total_commands_sent++;
        MCU_DEBUG("Flash memory flush sent successfully");
    } else {
        g_mcu_ctx.command_errors++;
        MCU_ERROR("Failed to send flash memory flush");
    }
    return ret;
}
#endif
