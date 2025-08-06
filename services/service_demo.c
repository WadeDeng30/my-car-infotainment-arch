// --- START OF FILE service_demo.c ---

#include "service_demo.h"
#include "service_timer.h" // 引入定时器服务
#include "bus_router.h"
#include <stdio.h>
#include <string.h>

// --- 调试宏 ---
static int g_demo_service_debug = 1;
static int g_demo_service_info = 1;
#define DEMO_DEBUG(fmt, ...) do { if (g_demo_service_debug) { printf("[DemoService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } } while(0)
#define DEMO_INFO(fmt, ...)  do { if (g_demo_service_info) { printf("[DemoService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } } while(0)
#define DEMO_ERROR(fmt, ...) do { printf("[DemoService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); } } while(0)

// --- 内部定义 ---
#define DEMO_QUEUE_SIZE 64

typedef struct {
    demo_internal_timer_type_t type;
    uint32_t                   timer_id;
} active_timer_info_t;

typedef struct {
    demo_state_t current_state;
    msg_payload_service_demo_t service_data;
    active_timer_info_t active_timers[DEMO_TIMER_TYPE_COUNT];
} demo_service_context_t;

// --- 静态变量 ---
static void* g_demo_queue_handle = NULL;
static int g_demo_thread_handle = 0;
static void* g_demo_mutex = NULL; // 用于保护上下文的互斥锁 (如果需要跨线程访问上下文)
static demo_service_context_t g_demo_ctx;

// --- 静态函数声明 ---
static void demo_thread_entry(void* p_arg);
static void demo_event_handler(app_message_t* msg);
static void demo_enter_state(demo_state_t new_state);
static void demo_exit_state(demo_state_t old_state);
static const char* demo_state_to_string(demo_state_t state);
static void demo_timer_start(demo_internal_timer_type_t timer_type, uint32_t duration_ms, timer_type_t type, unsigned int target_command, void* user_data);
static void demo_timer_stop(demo_internal_timer_type_t timer_type);
static void demo_timer_stop_all(void);

// --- 调试接口 ---
void service_demo_set_debug(int enable) { g_demo_service_debug = enable; }
void service_demo_set_info(int enable) { g_demo_service_info = enable; }

// --- Payload 管理 ---
msg_payload_service_demo_t* service_demo_payload_alloc(void) {
    // TODO: 如果需要复杂的内存池管理，可以在这里实现
    return (msg_payload_service_demo_t*)infotainment_malloc(sizeof(msg_payload_service_demo_t));
}

void service_demo_payload_free(void* payload) {
    if (payload) {
        infotainment_free(payload);
    }
}

msg_payload_service_demo_t* service_demo_payload_copy(const void* src) {
    if (!src) return NULL;
    msg_payload_service_demo_t* dst = service_demo_payload_alloc();
    if (dst) {
        memcpy(dst, src, sizeof(msg_payload_service_demo_t));
    }
    return dst;
}

// --- 服务初始化/退出 ---
void service_demo_init(void) {
    DEMO_INFO("Initializing...");
    
    // 初始化服务上下文
    memset(&g_demo_ctx, 0, sizeof(g_demo_ctx));
    g_demo_ctx.current_state = DEMO_STATE_UNINITIALIZED;
    // TODO: 设置 service_data 的默认值

    g_demo_thread_handle = infotainment_thread_create(
        demo_thread_entry, NULL, TASK_PROI_LEVEL3, 1024 * 4, "ServiceDemoThread"
    );
    if (g_demo_thread_handle <= 0) {
        DEMO_ERROR("Failed to create thread");
    }
}

void service_demo_exit(void) {
    DEMO_INFO("Exiting...");
    if (g_demo_thread_handle > 0) {
        // ... 实现优雅的线程退出逻辑 ...
    }
}

// --- 服务主线程 ---
static void demo_thread_entry(void* p_arg) {
    unsigned char err;
    DEMO_INFO("Thread started.");

    g_demo_queue_handle = infotainment_queue_create(DEMO_QUEUE_SIZE);
    if (!g_demo_queue_handle) { /* handle error */ return; }

    if (bus_router_register_service(SRC_DEMO_SERVICE, ADDRESS_DEMO_SERVICE, g_demo_queue_handle, "DemoService",service_demo_payload_free,service_demo_payload_copy) < 0) {
        /* handle error */ return;
    }

    // TODO: 执行任何需要硬件或底层依赖的初始化
    // ...

    // 发送就绪消息
    msg_payload_service_demo_t* ready_payload = service_demo_payload_copy(&g_demo_ctx.service_data);
    bus_post_message(SRC_DEMO_SERVICE, ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL, DEMO_EVT_READY, ready_payload, SRC_DEMO_SERVICE, NULL);

    // 进入初始状态
    demo_enter_state(DEMO_STATE_IDLE);

    while (1) {
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            demo_exit_state(g_demo_ctx.current_state);
            demo_timer_stop_all();
            goto exit;
        }

        app_message_t* msg = (app_message_t*)infotainment_queue_pend(g_demo_queue_handle, 0, &err);
        if (!msg || err != OS_NO_ERR) continue;
        
        demo_event_handler(msg);
        
        if (msg->payload) {
            free_ref_counted_payload((ref_counted_payload_t*)msg->payload);
        }
        infotainment_free(msg);
    }

exit:
    DEMO_INFO("Thread exiting...");
    bus_router_unregister_service(SRC_DEMO_SERVICE);
    // ... 清理队列等资源 ...
    infotainment_thread_delete(EXEC_prioself);
}


// --- 核心：状态机事件处理器 ---
static void demo_event_handler(app_message_t* msg) {
    ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
    void* payload_ptr = wrapper ? wrapper->ptr : NULL;

    DEMO_DEBUG("State:[%s], Event:[0x%X] from:[0x%X]", demo_state_to_string(g_demo_ctx.current_state), msg->command, msg->source);

    // 1. 处理全局命令
    if (msg->command == CMD_PING_REQUEST || msg->command == CMD_PING_BROADCAST) {
        service_handle_ping_request(msg);
        return;
    }
    else if (msg->command == CMD_PONG_RESPONSE) {
        service_handle_pong_response(msg);
        return;
    }   

    // 2. 按状态分发
    switch (g_demo_ctx.current_state) {
        case DEMO_STATE_IDLE:
            switch (msg->source) {
                case SRC_UI:
                    switch (msg->command) {
                        case DEMO_CMD_START_TASK:
                            demo_exit_state(DEMO_STATE_IDLE);
                            // TODO: 执行开始任务的动作
                            DEMO_INFO("Task started by UI command.");
                            demo_enter_state(DEMO_STATE_PROCESSING);
                            break;
                    }
                    break;
            }
            break;

        case DEMO_STATE_PROCESSING:
            switch (msg->source) {
                case SRC_TIMER_SERVICE: // 或者 msg->source 是自己
                    switch (msg->command) {
                        case DEMO_CMD_INTERNAL_TASK_TIMEOUT:
                            DEMO_ERROR("Task timed out!");
                            demo_exit_state(DEMO_STATE_PROCESSING);
                            // TODO: 执行超时处理
                            demo_enter_state(DEMO_STATE_ERROR);
                            break;
                    }
                    break;
                
                // 假设硬件完成后会给自己发消息
                case SRC_DEMO_SERVICE:
                    // ...
                    break;
            }
            break;
        
        // ... 其他状态 ...
        default:
            break;
    }
}


// --- 状态机 Enter/Exit 函数 ---
static void demo_enter_state(demo_state_t new_state) {
    DEMO_INFO("Entering state: %s", demo_state_to_string(new_state));
    g_demo_ctx.current_state = new_state;
    
    // 通知UI状态已改变
    msg_payload_service_demo_t* payload = service_demo_payload_alloc();
    if (payload) {
        payload->current_state = new_state;
        bus_post_message(SRC_DEMO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, DEMO_EVT_STATE_CHANGED, payload, SRC_DEMO_SERVICE, NULL);
    }

    switch (new_state) {
        case DEMO_STATE_PROCESSING:
            // 进入处理状态时，启动一个超时定时器
            demo_timer_start(DEMO_TIMER_TASK_TIMEOUT, 5000, TIMER_TYPE_ONE_SHOT, DEMO_CMD_INTERNAL_TASK_TIMEOUT, NULL);
            break;
        // ...
        default: break;
    }
}

static void demo_exit_state(demo_state_t old_state) {
    DEMO_INFO("Exiting state: %s", demo_state_to_string(old_state));
    switch (old_state) {
        case DEMO_STATE_PROCESSING:
            // 离开处理状态时，必须停止超时定时器
            demo_timer_stop(DEMO_TIMER_TASK_TIMEOUT);
            break;
        // ...
        default: break;
    }
}


// --- 定时器管理辅助函数 ---
static void demo_timer_start(demo_internal_timer_type_t timer_type, uint32_t duration_ms, timer_type_t type, unsigned int target_command, void* user_data) {
    demo_timer_stop(timer_type);
    uint32_t new_id = timer_start(duration_ms, type, ADDRESS_DEMO_SERVICE, target_command, user_data, user_data ? SRC_DEMO_SERVICE : SRC_UNDEFINED);
    if (new_id != TIMER_SERVICE_INVALID_ID) {
        for (int i = 0; i < DEMO_TIMER_TYPE_COUNT; i++) {
            if (g_demo_ctx.active_timers[i].type == DEMO_TIMER_NONE) {
                g_demo_ctx.active_timers[i].type = timer_type;
                g_demo_ctx.active_timers[i].timer_id = new_id;
                return;
            }
        }
        timer_stop(new_id);
    }
}

static void demo_timer_stop(demo_internal_timer_type_t timer_type) {
    for (int i = 0; i < DEMO_TIMER_TYPE_COUNT; i++) {
        if (g_demo_ctx.active_timers[i].type == timer_type) {
            if (g_demo_ctx.active_timers[i].timer_id != TIMER_SERVICE_INVALID_ID) {
                timer_stop(g_demo_ctx.active_timers[i].timer_id);
            }
            g_demo_ctx.active_timers[i].type = DEMO_TIMER_NONE;
            g_demo_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
            return;
        }
    }
}

static void demo_timer_stop_all(void) {
    for (int i = 0; i < DEMO_TIMER_TYPE_COUNT; i++) {
        demo_timer_stop((demo_internal_timer_type_t)i);
    }
}

// --- 调试辅助函数 ---
static const char* demo_state_to_string(demo_state_t state) {
    switch (state) {
        case DEMO_STATE_UNINITIALIZED: return "UNINITIALIZED";
        case DEMO_STATE_IDLE:          return "IDLE";
        case DEMO_STATE_ACTIVE:        return "ACTIVE";
        case DEMO_STATE_PROCESSING:    return "PROCESSING";
        case DEMO_STATE_ERROR:         return "ERROR";
        default:                       return "UNKNOWN";
    }
}