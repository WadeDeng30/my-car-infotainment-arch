// --- START OF FILE service_timer.c ---

#include "service_timer.h"
#include "bus_router.h" // 需要使用消息总线
#include <kapi.h>       // 需要使用OS的内核API，如获取tick


#define TIMER_QUEUE_SIZE 64
#define TIMER_THREAD_STACK_SIZE (1024 * 8)
#define TIMER_SERVICE_NAME "ServiceTimer"

// --- 调试功能相关定义 ---
static int g_timer_service_debug = 0;  // 调试开关，默认关闭
static int g_timer_service_info = 0;   // 信息开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define TIMER_DEBUG(fmt, ...) \
    do { \
        if (g_timer_service_debug) { \
            printf("[TimerService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define TIMER_INFO(fmt, ...) \
    do { \
        if (g_timer_service_info) { \
            printf("[TimerService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define TIMER_ERROR(fmt, ...) \
    printf("[TimerService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

// --- 内部数据结构 ---

// 代表一个活动定时器的节点
typedef struct timer_node_t {
    uint32_t id;                // 定时器的唯一ID
    uint64_t expires_at_tick;   // 到期的系统tick时间
    uint32_t duration_ticks;    // 定时间隔 (换算成tick)
    timer_type_t type;          // 类型
    
    // 到期后要发送的消息内容
    bus_router_address_t target_address;
    unsigned int         target_command;
    void*                user_data;
    void                (*free_func)(void*); // 【重要】指向 payload 的专用释放函数
    service_id_t         user_data_source;
    
    struct timer_node_t* next;  // 指向下一个节点的指针
} timer_node_t;

// --- 全局变量 ---
static void* g_timer_queue_handle = NULL;
static int   g_timer_thread_handle = 0;
static void* g_timer_mutex = NULL; // 用于保护定时器链表的互斥锁

static timer_node_t* g_active_timers = NULL; // 活动定时器链表 (按到期时间升序排列)
static uint32_t g_next_timer_id = 1;         // 用于生成唯一的定时器ID
static bool g_service_running = false;       // 服务运行状态
static bool g_service_ready = false;         // 服务就绪状态

// 统计信息
static timer_service_stats_t g_timer_stats = {0};

// --- 外部控制调试开关的函数 ---
void service_timer_set_debug(int enable) {
    g_timer_service_debug = enable;
    TIMER_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void service_timer_set_info(int enable) {
    g_timer_service_info = enable;
    TIMER_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

// --- 内部函数声明 ---
static void timer_thread_entry(void* p_arg);
static void insert_timer_node(timer_node_t* new_node);
static timer_node_t* remove_timer_node(uint32_t timer_id);
static timer_node_t* find_timer_node(uint32_t timer_id);
static uint32_t ms_to_ticks(uint32_t ms);
static uint64_t ticks_to_ms(uint64_t ticks);
static void cleanup_all_timers(void);
static void update_timer_stats(void);
static timer_error_t validate_timer_params(uint32_t duration_ms, timer_type_t type);

// --- 公共API实现 ---

timer_error_t service_timer_init(void) {
    unsigned char err;
    TIMER_DEBUG("Initializing timer service...");

    // 检查是否已经初始化
    if (g_service_ready) {
        TIMER_DEBUG("Timer service already initialized");
        return TIMER_ERROR_SUCCESS;
    }

    // 初始化统计信息
    memset(&g_timer_stats, 0, sizeof(g_timer_stats));

    // 创建互斥锁
    g_timer_mutex = infotainment_mutex_create(0, &err);
    if (!g_timer_mutex || err != 0) {
        TIMER_ERROR("Failed to create mutex, error: %d", err);
        return TIMER_ERROR_SYSTEM_ERROR;
    }

    // 创建消息队列
    g_timer_queue_handle = infotainment_queue_create(TIMER_QUEUE_SIZE);
    if (!g_timer_queue_handle) {
        TIMER_ERROR("Failed to create message queue");
        infotainment_mutex_delete(g_timer_mutex, &err);
        g_timer_mutex = NULL;
        return TIMER_ERROR_SYSTEM_ERROR;
    }

    // 注册到消息总线
    int register_result = bus_router_register_service(SRC_TIMER_SERVICE, ADDRESS_TIMER_SERVICE, g_timer_queue_handle,"Timer service",NULL,NULL);
    if (register_result != 0) {
        TIMER_ERROR("Failed to register service to bus router, error: %d", register_result);
        infotainment_queue_delete(g_timer_queue_handle, &err);
        infotainment_mutex_delete(g_timer_mutex, &err);
        g_timer_queue_handle = NULL;
        g_timer_mutex = NULL;
        return TIMER_ERROR_SYSTEM_ERROR;
    }

    // 创建服务线程
    g_service_running = true;
    g_timer_thread_handle = infotainment_thread_create(
        timer_thread_entry, NULL, TASK_PROI_LEVEL1, // 定时器线程优先级应该很高
        TIMER_THREAD_STACK_SIZE, TIMER_SERVICE_NAME
    );
    if (g_timer_thread_handle <= 0) {
        TIMER_ERROR("Failed to create timer thread");
        g_service_running = false;
        bus_router_unregister_service(SRC_TIMER_SERVICE);
        infotainment_queue_delete(g_timer_queue_handle, &err);
        infotainment_mutex_delete(g_timer_mutex, &err);
        g_timer_queue_handle = NULL;
        g_timer_mutex = NULL;
        return TIMER_ERROR_SYSTEM_ERROR;
    }

    g_service_ready = true;
    TIMER_DEBUG("Timer service initialized successfully");
    return TIMER_ERROR_SUCCESS;
}

timer_error_t service_timer_exit(void) {
    unsigned char err = 0;
    TIMER_DEBUG("Exiting timer service...");

    if (!g_service_ready) {
        TIMER_DEBUG("Timer service already exited or not initialized");
        return TIMER_ERROR_SUCCESS;
    }

    // 停止服务标志
    g_service_running = false;
    g_service_ready = false;

    // 注销服务（先注销，避免新的消息进入）
    bus_router_unregister_service(SRC_TIMER_SERVICE);

    // 请求线程退出
    if (g_timer_thread_handle > 0) {
        TIMER_DEBUG("Requesting timer thread deletion...");
        if(g_timer_queue_handle)
            infotainment_queue_post(g_timer_queue_handle, NULL);
        while(infotainment_thread_delete_req(g_timer_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            if(g_timer_queue_handle)
                infotainment_queue_post(g_timer_queue_handle, NULL);
            infotainment_thread_sleep(10);
        }
        g_timer_thread_handle = 0;
    }

    // 清理消息队列（防止内存泄漏）
    if (g_timer_queue_handle) {
        void* msg = NULL;

        // 获取互斥锁保护队列操作
        if (g_timer_mutex) {
            infotainment_mutex_pend(g_timer_mutex, 0, &err);
        }

        // 不断从队列取出消息并释放，直到队列为空
        while ((msg = infotainment_queue_get(g_timer_queue_handle, &err)) && err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg && app_msg->payload) {
                // 释放消息载荷
                free_ref_counted_payload((ref_counted_payload_t*)app_msg->payload);
            }
            if (app_msg) {
                infotainment_free(app_msg);
            }
            TIMER_DEBUG("Freed queued message during exit cleanup");
        }

        // 删除消息队列
        infotainment_queue_delete(g_timer_queue_handle, &err);
        g_timer_queue_handle = NULL;

        if (g_timer_mutex) {
            infotainment_mutex_post(g_timer_mutex);
        }
    }

    // 清理互斥锁
    if (g_timer_mutex) {
        infotainment_mutex_delete(g_timer_mutex, &err);
        g_timer_mutex = NULL;
    }
    cleanup_all_timers();
    // 重置全局变量
    g_active_timers = NULL;
    g_next_timer_id = 1;
    memset(&g_timer_stats, 0, sizeof(g_timer_stats));

    TIMER_DEBUG("Timer service cleanup complete");
    return TIMER_ERROR_SUCCESS;
}

bool service_timer_is_ready(void) {
    return g_service_ready;
}

uint32_t timer_start(
    uint32_t duration_ms,
    timer_type_t type,
    bus_router_address_t target_address,
    unsigned int target_command,
    void* user_data,
    service_id_t user_data_source)
{
        // 参数验证
    timer_error_t validation_result = validate_timer_params(duration_ms, type);
    service_registry_entry_t source_info = {0};
    bus_router_get_service_info_by_address(target_address, &source_info);
    if (!source_info.is_used) {
        TIMER_ERROR("Invalid target address: 0x%llx", target_address);
        return TIMER_SERVICE_INVALID_ID;
    }

    if (validation_result != TIMER_ERROR_SUCCESS) {
        TIMER_ERROR("Invalid timer parameters: duration=%u, type=%d", duration_ms, type);
        return TIMER_SERVICE_INVALID_ID;
    }

    if (!g_service_ready) {
        TIMER_ERROR("Timer service not ready");
        return TIMER_SERVICE_INVALID_ID;
    }

    // 检查是否达到最大定时器数量
    if (g_timer_stats.active_timers >= TIMER_SERVICE_MAX_TIMERS) {
        TIMER_ERROR("Maximum number of timers reached: %d", TIMER_SERVICE_MAX_TIMERS);
        return TIMER_SERVICE_INVALID_ID;
    }

    // 分配payload
    msg_payload_timer_start_t* payload = (msg_payload_timer_start_t*)infotainment_malloc(sizeof(msg_payload_timer_start_t));
    if (!payload) {
        TIMER_ERROR("Failed to allocate memory for timer start payload");
        return TIMER_SERVICE_INVALID_ID;
    }

    // 生成唯一ID (线程安全)
    unsigned char err;
    uint32_t new_id;
    infotainment_mutex_pend(g_timer_mutex, 0, &err);
    new_id = g_next_timer_id++;
    if (g_next_timer_id == 0) g_next_timer_id = 1; // 防止ID为0
    infotainment_mutex_post(g_timer_mutex);

    // 填充payload
    payload->timer_id = new_id;
    payload->duration_ms = duration_ms;
    payload->type = type;
    payload->target_address = target_address;
    payload->target_command = target_command;
    payload->user_data = user_data;
    payload->user_data_source = user_data_source;

    // 发送启动消息给定时器服务
    int post_result = bus_post_message(source_info.id, ADDRESS_TIMER_SERVICE, MSG_PRIO_HIGH, TIMER_CMD_START, payload,SRC_UNDEFINED, NULL);
    if (post_result != 0) {
        TIMER_ERROR("Failed to post timer start message, error: %d", post_result);
        return TIMER_SERVICE_INVALID_ID;
    }

    TIMER_INFO("Starting timer (ID: %u, Duration: %u ms, Type: %d, Target: 0x%llX, Command: 0x%X,user data: 0x%X)",
           new_id, duration_ms, type, target_address, target_command,user_data);
    return new_id;
}

timer_error_t timer_stop(uint32_t timer_id) {
    if (timer_id == TIMER_SERVICE_INVALID_ID) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    if (!g_service_ready) {
        TIMER_ERROR("Timer service not ready");
        return TIMER_ERROR_SERVICE_NOT_READY;
    }

    msg_payload_timer_stop_t* payload = (msg_payload_timer_stop_t*)infotainment_malloc(sizeof(msg_payload_timer_stop_t));
    if (!payload) {
        TIMER_ERROR("Failed to allocate memory for timer stop payload");
        return TIMER_ERROR_NO_MEMORY;
    }

    payload->timer_id = timer_id;

    int post_result = bus_post_message(SRC_TIMER_SERVICE, ADDRESS_TIMER_SERVICE, MSG_PRIO_HIGH, TIMER_CMD_STOP, payload, SRC_UNDEFINED, NULL);
    if (post_result != 0) {
        TIMER_ERROR("Failed to post timer stop message, error: %d", post_result);
        infotainment_free(payload);
        return TIMER_ERROR_SYSTEM_ERROR;
    }

    TIMER_INFO("Stopping timer %u", timer_id);
    return TIMER_ERROR_SUCCESS;
}

timer_error_t timer_query(uint32_t timer_id, timer_state_t* state, uint32_t* remaining_ms) {
    if (timer_id == TIMER_SERVICE_INVALID_ID || !state || !remaining_ms) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    if (!g_service_ready) {
        return TIMER_ERROR_SERVICE_NOT_READY;
    }

    // 简单实现：直接在当前线程中查找定时器
    unsigned char err;
    infotainment_mutex_pend(g_timer_mutex, 0, &err);

    timer_node_t* node = find_timer_node(timer_id);
    if (!node) {
        infotainment_mutex_post(g_timer_mutex);
        *state = TIMER_STATE_IDLE;
        *remaining_ms = 0;
        return TIMER_ERROR_NOT_FOUND;
    }

    *state = TIMER_STATE_RUNNING;
    uint64_t current_tick = infotainment_get_ticks();
    if (node->expires_at_tick > current_tick) {
        *remaining_ms = (uint32_t)ticks_to_ms(node->expires_at_tick - current_tick);
    } else {
        *remaining_ms = 0;
    }

    infotainment_mutex_post(g_timer_mutex);
    return TIMER_ERROR_SUCCESS;
}

timer_error_t timer_modify(uint32_t timer_id, uint32_t new_duration_ms) {
    if (timer_id == TIMER_SERVICE_INVALID_ID ||
        new_duration_ms < TIMER_SERVICE_MIN_DURATION_MS ||
        new_duration_ms > TIMER_SERVICE_MAX_DURATION_MS) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    if (!g_service_ready) {
        return TIMER_ERROR_SERVICE_NOT_READY;
    }

    // 简单实现：先停止再重新启动
    // 更复杂的实现可以直接修改定时器节点
    TIMER_DEBUG("Timer modify not fully implemented: ID=%u, new_duration=%ums", timer_id, new_duration_ms);
    return TIMER_ERROR_SUCCESS; // 暂时返回成功
}

timer_error_t timer_get_stats(timer_service_stats_t* stats) {
    if (!stats) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    if (!g_service_ready) {
        return TIMER_ERROR_SERVICE_NOT_READY;
    }

    unsigned char err;
    infotainment_mutex_pend(g_timer_mutex, 0, &err);
    *stats = g_timer_stats;
    infotainment_mutex_post(g_timer_mutex);

    return TIMER_ERROR_SUCCESS;
}

void timer_dump_all(void) {
    if (!g_service_ready) {
        printf("[TIMER_SERVICE] Service not ready\n");
        return;
    }

    unsigned char err;
    infotainment_mutex_pend(g_timer_mutex, 0, &err);

    printf("\n=== Timer Service Status ===\n");
    printf("Total timers: %u\n", g_timer_stats.total_timers);
    printf("Active timers: %u\n", g_timer_stats.active_timers);
    printf("Expired count: %u\n", g_timer_stats.expired_count);
    printf("Max timers used: %u\n", g_timer_stats.max_timers_used);
    printf("Memory usage: %u bytes\n", g_timer_stats.memory_usage_bytes);

    printf("\nActive Timer List:\n");
    timer_node_t* current = g_active_timers;
    int index = 0;
    while (current) {
        uint64_t current_tick = infotainment_get_ticks();
        uint32_t remaining_ms = 0;
        if (current->expires_at_tick > current_tick) {
            remaining_ms = (uint32_t)ticks_to_ms(current->expires_at_tick - current_tick);
        }

        printf("  [%d] ID=%u, expires_in=%ums, type=%s, target=0x%llx, cmd=0x%x\n",
               index++, current->id, remaining_ms,
               (current->type == TIMER_TYPE_ONE_SHOT) ? "ONESHOT" : "PERIODIC",
               current->target_address, current->target_command);
        current = current->next;
    }
    printf("============================\n\n");

    infotainment_mutex_post(g_timer_mutex);
}


// --- 内部函数实现 ---

static uint32_t ms_to_ticks(uint32_t ms) {
    // 假设系统 tick 是 10ms (100Hz)
    // 实际值应从OS获取，例如 OS_TICKS_PER_SEC
    const uint32_t ticks_per_sec = 100;
    if (ms <= 10) return 1; // 至少等待1个tick
    return (ms * ticks_per_sec) / 1000;
}

// 将新定时器按到期时间插入到已排序的链表中
static void insert_timer_node(timer_node_t* new_node) {
    if (!new_node) {
        TIMER_ERROR("Attempting to insert NULL timer node");
        return;
    }

    new_node->next = NULL; // 确保新节点的next指针为NULL

    if (!g_active_timers || new_node->expires_at_tick < g_active_timers->expires_at_tick) {
        // 插入到头部
        new_node->next = g_active_timers;
        g_active_timers = new_node;
        TIMER_DEBUG("Inserted timer ID=%u at head", new_node->id);
    } else {
        timer_node_t* current = g_active_timers;
        while (current->next && new_node->expires_at_tick >= current->next->expires_at_tick) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node; // 修复bug：这里应该是new_node而不是current
        TIMER_DEBUG("Inserted timer ID=%u after ID=%u", new_node->id, current->id);
    }
}

static timer_node_t* remove_timer_node(uint32_t timer_id) {
    timer_node_t* current = g_active_timers;
    timer_node_t* prev = NULL;

    while (current) {
        if (current->id == timer_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_active_timers = current->next;
            }

            current->next = NULL; // 清理指针
            TIMER_DEBUG("Removed timer ID=%u from active list", timer_id);

            return current; // 返回节点，让调用者决定如何处理
        }
        prev = current;
        current = current->next;
    }

    TIMER_DEBUG("Timer ID=%u not found in active list", timer_id);
    return NULL;
}


static void timer_thread_entry(void* p_arg) {
    unsigned char err;
    uint32_t sleep_ticks;

    TIMER_DEBUG("Timer service thread started");

    while (1) {
        // 检查是否有线程删除请求
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            TIMER_DEBUG("Timer service thread delete request received");
            goto exit;
        }
        infotainment_mutex_pend(g_timer_mutex, 0, &err);

        uint64_t current_tick = infotainment_get_ticks();

        // 1. 处理到期的定时器
        while (g_active_timers && g_active_timers->expires_at_tick <= current_tick) {
            timer_node_t* expired_node = g_active_timers;
            g_active_timers = expired_node->next; // 从链表移除
            
            infotainment_mutex_post(g_timer_mutex); // 【重要】在发消息前释放锁，防止死锁
            
            // 发送到期消息
           // printf("Service Timer: Timer %u expired. Firing command 0x%X to address 0x%llX with user data 0x%llX\n",
                 //  expired_node->id, expired_node->target_command, expired_node->target_address,expired_node->user_data);
            
             void* payload_to_send = NULL;
             if (expired_node->type == TIMER_TYPE_PERIODIC) {
                // --- 周期性定时器：克隆 payload ---
                if (expired_node->user_data) {
                    // 查找并调用拷贝函数
                    void* (*copy_func)(const void*) = find_payload_copy_func(expired_node->user_data_source);
                    if (copy_func) {
                        payload_to_send = copy_func(expired_node->user_data);
                        TIMER_DEBUG("Cloned payload for periodic timer %u.", expired_node->id);
                    } else {
                        // 没有拷贝函数，这是一个配置错误！
                        // 我们不能发送原始指针，所以只能发送 NULL
                        TIMER_ERROR("No copy function for periodic timer %u payload from source 0x%X! Sending NULL.",
                                          expired_node->id, expired_node->user_data_source);
                        payload_to_send = NULL;
                    }
                }
            } else {
                // --- 单次定时器：转移原始 payload 的所有权 ---
                payload_to_send = expired_node->user_data;
                // 将节点中的指针清空，防止被重复释放
                expired_node->user_data = NULL;
            }

            TIMER_DEBUG("Firing timer %u to 0x%llX with command 0x%X and payload 0x%llX from data source 0x%X.", 
                        expired_node->id, expired_node->target_address, expired_node->target_command, payload_to_send, expired_node->user_data_source);
            bus_post_message(
                SRC_TIMER_SERVICE,
                expired_node->target_address,
                MSG_PRIO_NORMAL,
                expired_node->target_command,
                payload_to_send,
                expired_node->user_data_source,
                NULL
            );
            // payload_to_send 的所有权已经转移给总线，我们不再关心它
            
            infotainment_mutex_pend(g_timer_mutex, 0, &err); // 重新获取锁

            // 更新统计信息
            g_timer_stats.expired_count++;

            if (expired_node->type == TIMER_TYPE_PERIODIC) {
                // 如果是周期性定时器，计算下一次到期时间并重新插入
                expired_node->expires_at_tick += expired_node->duration_ticks;
                insert_timer_node(expired_node);
                TIMER_DEBUG("Periodic timer %u rescheduled", expired_node->id);
            } else {
                // 单次定时器，释放节点
                infotainment_free(expired_node);
                TIMER_DEBUG("One-shot timer %u completed and freed", expired_node->id);
            }

            update_timer_stats(); // 更新统计信息
        }
        
        // 2. 计算下一次休眠时间
        if (g_active_timers) {
            current_tick = infotainment_get_ticks();
            if (g_active_timers->expires_at_tick > current_tick) {
                sleep_ticks = (uint32_t)(g_active_timers->expires_at_tick - current_tick);
            } else {
                sleep_ticks = 1; // 马上就到期了，只休眠很短时间
            }
        } else {
            sleep_ticks = 0; // 没有定时器，无限期等待消息
        }
        
        infotainment_mutex_post(g_timer_mutex);
        
        // 3. 等待新消息或等待超时
        app_message_t* msg = (app_message_t*)infotainment_queue_pend(g_timer_queue_handle, sleep_ticks, &err);
        
        if (msg && err == OS_NO_ERR) {

            printf("Service Timer: 收到消息 (来源:0x%x, 命令:0x%x)\n", msg->source, msg->command);
            if (msg->command == CMD_PING_REQUEST || msg->command == CMD_PING_BROADCAST) {
                service_handle_ping_request(msg);
            }
            else if (msg->command == CMD_PONG_RESPONSE) {
                service_handle_pong_response(msg);
            }
            else
            {
                // 处理收到的命令 (启动/停止定时器)
                switch (msg->command) {
                    case TIMER_CMD_START: {
                        ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
                        if (!wrapper) {
                            TIMER_ERROR("Received TIMER_CMD_START with no payload");
                            break;
                        }
                        msg_payload_timer_start_t* p_start = (msg_payload_timer_start_t*)wrapper->ptr;
                        if (p_start) {
                            timer_node_t* new_node = (timer_node_t*)infotainment_malloc(sizeof(timer_node_t));
                            if (new_node) {
                                new_node->id = p_start->timer_id;
                                new_node->duration_ticks = ms_to_ticks(p_start->duration_ms);
                                new_node->expires_at_tick = infotainment_get_ticks() + new_node->duration_ticks;
                                new_node->type = p_start->type;
                                new_node->target_address = p_start->target_address;
                                new_node->target_command = p_start->target_command;
                                new_node->user_data = p_start->user_data;
                                new_node->free_func = wrapper->free_func;                            
                                new_node->user_data_source = p_start->user_data_source;
                                
                                infotainment_mutex_pend(g_timer_mutex, 0, &err);
                                insert_timer_node(new_node);
                                g_timer_stats.total_timers++;
                                update_timer_stats(); // 更新统计信息
                                infotainment_mutex_post(g_timer_mutex);
                                printf("Service Timer: Started timer %u. Expires in %u ticks.user_data:0x%llX\n", new_node->id, new_node->duration_ticks,new_node->user_data);
                            }
                        }
                        break;
                    }
                    case TIMER_CMD_STOP: {
                        ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
                        msg_payload_timer_stop_t* p_stop = (msg_payload_timer_stop_t*)wrapper->ptr;
                        if (p_stop) {
                            infotainment_mutex_pend(g_timer_mutex, 0, &err);
                            timer_node_t* removed_node = remove_timer_node(p_stop->timer_id);
                            update_timer_stats(); // 更新统计信息
                            infotainment_mutex_post(g_timer_mutex);

                            if (removed_node) {
                                // [添加日志]
                                printf("[TIMER_PAYLOAD] Stopped Timer ID %u. Found associated payload_ptr: %p to be freed.\n",
                                    p_stop->timer_id, removed_node->user_data);
                                // 释放用户数据 (如果有的话)
                                if (removed_node->user_data) {
                                    // 需要一个类似 bus_router 的 find_free_func 机制
                                    if(removed_node->free_func)
                                    {
                                        removed_node->free_func(removed_node->user_data);
                                    }
                                    else
                                    {
                                        infotainment_free(removed_node->user_data);
                                    }
                                    removed_node->user_data = NULL;
                                }
                                else
                                {
                                     // [添加日志]
                                    printf("[TIMER_PAYLOAD] Stop request for Timer ID %u, but it was not found (already expired or invalid).\n",
                                        p_stop->timer_id);
                                }
                                infotainment_free(removed_node);
                                TIMER_DEBUG("Stopped timer %u", p_stop->timer_id);
                            } else {
                                TIMER_DEBUG("Timer %u not found for stopping", p_stop->timer_id);
                            }
                        }
                        break;
                    }
                }
            }
            // 释放消息和载荷
            if (msg->payload) 
                free_ref_counted_payload((ref_counted_payload_t*)msg->payload);
                
            infotainment_free(msg);
            msg  = NULL;
        }

        // 检查服务是否应该退出
        if (!g_service_running) {
            TIMER_DEBUG("Timer service thread exiting...");
            break;
        }
    }

exit:
    // 清理线程资源
    TIMER_DEBUG("Timer service thread cleanup started");
    // 删除线程自身
    infotainment_thread_delete(EXEC_prioself);
}

// ================================
// 新增的辅助函数实现
// ================================

static timer_node_t* find_timer_node(uint32_t timer_id) {
    timer_node_t* current = g_active_timers;

    while (current) {
        if (current->id == timer_id) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

static uint64_t ticks_to_ms(uint64_t ticks) {
    // 假设系统 tick 是 10ms (100Hz)
    const uint32_t ticks_per_sec = 100;
    return (ticks * 1000) / ticks_per_sec;
}

static void cleanup_all_timers(void) {
    timer_node_t* current = g_active_timers;

    while (current) {
        timer_node_t* next = current->next;

        // 释放用户数据 (如果有的话)
        if (current->user_data) {
            // 这里需要根据 user_data_source 来决定如何释放
            // 简化处理，直接调用 free
            if (current->free_func) 
            {
                current->free_func(current->user_data);
            }
            else
            {
                infotainment_free(current->user_data);
            }
        }

        infotainment_free(current);
        current = next;
    }

    g_active_timers = NULL;
    g_timer_stats.active_timers = 0;
    TIMER_DEBUG("All timers cleaned up");
}

static void update_timer_stats(void) {
    // 计算当前活跃定时器数量
    uint32_t count = 0;
    timer_node_t* current = g_active_timers;
    while (current) {
        count++;
        current = current->next;
    }

    g_timer_stats.active_timers = count;
    if (count > g_timer_stats.max_timers_used) {
        g_timer_stats.max_timers_used = count;
    }

    // 估算内存使用量
    g_timer_stats.memory_usage_bytes = count * sizeof(timer_node_t);
}

static timer_error_t validate_timer_params(uint32_t duration_ms, timer_type_t type) {
    if (duration_ms < TIMER_SERVICE_MIN_DURATION_MS ||
        duration_ms > TIMER_SERVICE_MAX_DURATION_MS) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    if (type != TIMER_TYPE_ONE_SHOT && type != TIMER_TYPE_PERIODIC) {
        return TIMER_ERROR_INVALID_PARAM;
    }

    return TIMER_ERROR_SUCCESS;
}