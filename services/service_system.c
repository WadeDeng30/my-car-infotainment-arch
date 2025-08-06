#include <service_system.h>
#include <elibs_stdio.h>
#include <stdbool.h>
#include <sys/_types.h>
#include <ui_screens.h>
#define printf eLIBs_printf

// --- 调试功能相关定义 ---
static int g_system_service_debug = 1;  // 调试开关，默认关闭
static int g_system_service_info = 1;   // 信息开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define SYS_DEBUG(fmt, ...) \
    do { \
        if (g_system_service_debug) { \
            printf("[SystemService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define SYS_INFO(fmt, ...) \
    do { \
        if (g_system_service_info) { \
            printf("[SystemService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define SYS_ERROR(fmt, ...) \
    printf("[SystemService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

static void* service_system_queue_handle = 0;
static int service_system_thread_handle = 0;
#define SYSTEM_QUEUE_SIZE 128
static int service_system_payload_use_count = 0;
static void* service_system_semaphore = NULL;


// --- 全局变量 ---
static bus_router_address_t g_ready_services_mask = ADDRESS_NONE;
static bool g_boot_notification_sent = false;



typedef struct {
    /**
     * @brief 原始的显示意图。
     */
    display_intent_t intent;
    
    /**
     * @brief 发起这个意图请求的服务ID。
     * 用于在请求结束时，知道是谁需要被移除。
     */
    service_id_t     requester_id;
    
    /**
     * @brief 这个意图的显示优先级。
     * 用于在多个意图冲突时进行仲裁。
     */
    int              priority;
    
    /**
     * @brief 标记这个意图是否允许被用户的常规导航操作最小化。
     * 例如，倒车影像为 false，电话为 true。
     */
    bool             is_minimizable;
    
    /**
     * @brief 运行时的状态：标记这个意图当前是否已被用户最小化。
     */
    bool             is_minimized;
    
} active_intent_t;

// --- 内部数据结构 ---
typedef struct {
    display_intent_t intent;         // 保存完整的意图
    service_id_t     requester_id;
    int              priority;
    bool             is_minimizable;
    bool             is_minimized;
} force_stack_item_t;

// 用户层的内容请求
typedef struct {
    display_intent_t intent;
    service_id_t     requester_id;
    int              priority;
} user_content_request_t;

// --- 内部常量定义 ---
#define MAX_FORCE_STACK_DEPTH 5    // 强制显示栈的最大深度
#define MAX_USER_CONTENT_REQUESTS 5 // 用户层内容请求的最大数量

// 优先级定义
#define PRIORITY_SAFETY_CRITICAL 100
#define PRIORITY_USER_NORMAL     10




// --- 内部函数声明 ---
static void re_evaluate_and_switch_display(void);
static void broadcast_forced_stack_state(void);


// 【新增】定义一个结构体来保存单个定时器的信息
typedef struct {
    system_internal_timer_type_t type;     // 定时器的用途
    uint32_t                     timer_id; // 从 service_timer 返回的ID
} system_active_timer_info_t;

// 【新增】System 服务上下文结构体
typedef struct {
    // 使用一个固定大小的数组来管理所有活动定时器
    // SYSTEM_TIMER_TYPE_COUNT 保证了数组大小足够
    system_state_payload_t system_data;
    system_active_timer_info_t active_timers[SYSTEM_TIMER_TYPE_COUNT];
    const reg_system_para_t	   *system_reg_para;//服务注册表内存
    const reg_init_para_t	   *init_reg_para;//服务注册表内存
    const reg_root_para_t	   *root_reg_para;//服务注册表内存
    system_state_t current_state;
    // 系统状态相关
    bool ui_ready;
    uint32_t startup_time;
    uint32_t ping_test_count;

    // 强制层
    force_stack_item_t g_force_display_stack[MAX_FORCE_STACK_DEPTH];
    int g_force_display_stack_top;

    // 用户层
    ui_screen_id_t g_user_current_screen;
    ui_source_type_t g_user_current_source;
    user_content_request_t g_user_content_requests[MAX_USER_CONTENT_REQUESTS];
    int g_user_content_requests_count;


} system_service_context_t;

static system_service_context_t g_system_ctx; // 全局服务上下文实例

// --- 内部函数声明 ---
static void system_timer_start(system_internal_timer_type_t timer_type, uint32_t duration_ms,
                              timer_type_t type, unsigned int target_command, void* user_data);
static void system_timer_stop(system_internal_timer_type_t timer_type);
static void system_timer_stop_all(void);
static void system_handle_internal_timer_timeout(unsigned int command);

// --- 外部控制调试开关的函数 ---
void service_system_set_debug(int enable) {
    g_system_service_debug = enable;
    SYS_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void service_system_set_info(int enable) {
    g_system_service_info = enable;
    SYS_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

system_state_payload_t *service_system_payload_alloc(void) {
    system_state_payload_t* service_system_payload = NULL;
    unsigned char err = 0;
    if (service_system_semaphore)
        infotainment_sem_wait(service_system_semaphore, 0, &err);

    if (service_system_payload_use_count > SYSTEM_QUEUE_SIZE) {
        if (service_system_semaphore)
            infotainment_sem_post(service_system_semaphore);
        return NULL;
    }
    service_system_payload = (system_state_payload_t*)infotainment_malloc(sizeof(system_state_payload_t));
    if (!service_system_payload) {
        SYS_ERROR("Failed to allocate payload memory");
        if (service_system_semaphore)
            infotainment_sem_post(service_system_semaphore);
        return NULL; // 内存分配失败
    }
    service_system_payload_use_count++;
    if (service_system_semaphore)
        infotainment_sem_post(service_system_semaphore);
    return service_system_payload;
}


void service_system_payload_free(system_state_payload_t* service_system_payload) {
    unsigned char err = 0;
    if (service_system_semaphore)
        infotainment_sem_wait(service_system_semaphore, 0, &err);
    if (service_system_payload) {
        infotainment_free(service_system_payload);
        if (service_system_payload_use_count > 0) {
            service_system_payload_use_count--;
        }
    }
    service_system_payload = NULL;
    if (service_system_semaphore)
        infotainment_sem_post(service_system_semaphore);
    SYS_DEBUG("Payload memory freed");
}

system_state_payload_t* service_system_payload_copy(const system_state_payload_t* src) {
    if (!src) return NULL;
    system_state_payload_t* dst = service_system_payload_alloc();
    if (dst) {
        memcpy(dst, src, sizeof(system_state_payload_t));
    }
    return dst;
}

void service_system_update_payload_data(void)
{
    if(g_system_ctx.system_reg_para && \
        g_system_ctx.init_reg_para && \
        g_system_ctx.root_reg_para)
    {
        g_system_ctx.system_data.screen_id = g_system_ctx.root_reg_para->last_screen_id;
        g_system_ctx.system_data.initial_source_type = g_system_ctx.root_reg_para->last_work_source;
    }
}

/**
 * @brief 查找指定屏幕的依赖掩码
 */

// get_screen_priority 函数现在直接调用 ui_screens 提供的接口
int get_screen_priority(ui_screen_id_t screen_id) {
    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    return profile ? profile->priority : -1; // 如果找不到，返回最低优先级
}

static bus_router_address_t get_dependencies_for_screen(ui_screen_id_t screen_id) {

    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    return profile ? profile->required_mask : ADDRESS_SYSTEM_SERVICE; // 如果找不到，至少要等自己就绪

}

static uint32_t get_dependencies_timeout_for_screen(ui_screen_id_t screen_id) {

    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    return profile ? profile->required_timeout : 10; // 如果找不到，至少要等10ms
}


/**
 * @brief 检查目标屏幕的所有服务是否已就绪，并通知UI
 */
static void check_all_services_ready_and_notify_ui(void) {
    if (g_boot_notification_sent) {
        return; // 开机指令只发一次
    }

    // 1. 获取当前目标启动界面所需的依赖
    bus_router_address_t required_mask = get_dependencies_for_screen(g_system_ctx.system_data.screen_id);

    SYS_DEBUG("Checking ready services for screen %d. Current mask: 0x%llX, Required mask: 0x%llX",
               g_system_ctx.system_data.screen_id, g_ready_services_mask, required_mask);
    
    // 2. 使用位运算检查所有依赖是否满足
    if ((g_ready_services_mask & required_mask) == required_mask) {
        SYS_INFO("All required services for screen %d are ready. Sending OPEN_UI command.", g_system_ctx.system_data.screen_id);
        
         re_evaluate_and_switch_display();
         broadcast_forced_stack_state();

        #if 0
        // 准备 payload
        system_state_payload_t* payload = service_system_payload_copy(&g_system_ctx.system_data);
        if (payload) {
            bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH, SYSTEM_EVT_SYSTEM_OPEN_UI, payload, SRC_SYSTEM_SERVICE, NULL);
            g_boot_notification_sent = true; // 标记已发送
        }
        #endif    
    }
}

// --- START OF FILE service_system.c (Corrected Initialization) ---

// ... (includes, 内部数据结构定义等) ...

/**
 * @brief 【修正】根据一个布局配置，初始化用户层的状态
 * 
 * 这个函数现在会为布局中的 *每一个* 控件创建一个默认的内容请求，
 * 从而完整地代表了用户层的初始状态。
 */
static void initialize_user_layer_from_profile(ui_screen_id_t screen_id,ui_source_type_t source_type) {
    const layout_profile_t* profile = ui_screen_get_profile_by_id(screen_id);
    if (!profile) {
        SYS_ERROR("Cannot initialize user layer: Profile ID %d not found.", screen_id);
        g_system_ctx.g_user_current_screen = UI_SCREEN_HOME; // Fallback
        g_system_ctx.g_user_content_requests_count = 0;
        return;
    }

    g_system_ctx.g_user_current_screen = screen_id;
    g_system_ctx.g_user_content_requests_count = 0; // 清空旧的请求

    SYS_INFO("Initializing user layer for screen '%s' (ID:%d)...", profile->description, screen_id);

    // 【核心修正】遍历布局中的所有控件，并为它们创建意图
    for (int i = 0; i < profile->widget_count; ++i) {
        if (g_system_ctx.g_user_content_requests_count >= MAX_USER_CONTENT_REQUESTS) {
            SYS_ERROR("User content request list is full. Some widgets may not be initialized.");
            break;
        }

        user_content_request_t* req = &g_system_ctx.g_user_content_requests[g_system_ctx.g_user_content_requests_count++];
        
        // 填充意图
        req->intent.widget_id = profile->widgets[i].type;
        req->requester_id = SRC_SYSTEM_SERVICE; // 标记为系统默认意图
        req->intent.source_type = UI_SOURCE_TYPE_NONE; // 默认值

        // 为 SOURCE 控件特殊处理，从 create_arg 获取初始 source_type
        if (req->intent.widget_id == MANGER_ID_SOURCE) {
            req->intent.source_type = source_type;
        }
        
        // 所有用户层的意图，其显示优先级都应该是普通的
        req->priority = PRIORITY_USER_NORMAL;

        SYS_DEBUG("  -> Added user intent for widget %d (source_type: %d)", req->intent.widget_id, req->intent.source_type);
    }
}

// --- START OF FILE service_system.c (Corrected Helper Function) ---

/**
 * @brief 辅助函数：检查当前【实际显示】的布局是否包含某个 widget
 * 
 * @param widget_id 要检查的控件ID
 * @return 如果包含则为 true，否则为 false
 */
static bool currently_displayed_layout_contains_widget(ui_manger_id_t widget_id) {
    // 【核心修正】使用 g_system_ctx.system_data.screen_id 进行查找
    if (g_system_ctx.system_data.screen_id == UI_SCREEN_NONE) {
        return false;
    }

    const layout_profile_t* profile = ui_screen_get_profile_by_id(g_system_ctx.system_data.screen_id);
    if (!profile) {
        SYS_ERROR("Profile not found for currently displayed screen ID: %d", g_system_ctx.system_data.screen_id);
        return false;
    }

    for (int i = 0; i < profile->widget_count; ++i) {
        if (profile->widgets[i].type == widget_id) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief 【新增】将一个强制显示请求按优先级插入到栈中
 * 
 * 栈始终保持按优先级降序排列，最高优先级的在栈顶 (g_force_display_stack_top)。
 * 
 * @param new_request 要插入的新请求
 */
static void insert_to_force_stack(force_stack_item_t new_request) {
    // 检查请求者是否已在栈中，如果是，则更新而不是插入
    for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
        if (g_system_ctx.g_force_display_stack[i].requester_id == new_request.requester_id) {
            SYS_DEBUG("Updating existing force request for service 0x%X.", new_request.requester_id);
            g_system_ctx.g_force_display_stack[i] = new_request;
            // 更新后需要重新排序
            // (简单起见，先移除再重新插入)
            force_stack_item_t temp = g_system_ctx.g_force_display_stack[i];
            // 将后续元素前移
            for (int j = i; j < g_system_ctx.g_force_display_stack_top; j++) {
                g_system_ctx.g_force_display_stack[j] = g_system_ctx.g_force_display_stack[j + 1];
            }
            g_system_ctx.g_force_display_stack_top--;
            insert_to_force_stack(temp); // 递归调用以重新排序插入
            return;
        }
    }
    
    // 如果栈已满
    if (g_system_ctx.g_force_display_stack_top >= MAX_FORCE_STACK_DEPTH - 1) {
        // 检查新请求的优先级是否高于栈底（最低优先级）的请求
        if (new_request.priority > g_system_ctx.g_force_display_stack[0].priority) {
            SYS_INFO("Force stack is full. Ejecting lowest priority request (requester 0x%X).", g_system_ctx.g_force_display_stack[0].requester_id);
            // 弹出栈底元素
            for (int i = 0; i < g_system_ctx.g_force_display_stack_top; i++) {
                g_system_ctx.g_force_display_stack[i] = g_system_ctx.g_force_display_stack[i + 1];
            }
            g_system_ctx.g_force_display_stack_top--;
        } else {
            SYS_ERROR("Force stack is full and new request priority is too low. Request from 0x%X is rejected.", new_request.requester_id);
            return; // 拒绝新请求
        }
    }
    
    // 找到正确的插入位置 (从低到高)
    int insert_pos = 0;
    while (insert_pos <= g_system_ctx.g_force_display_stack_top && new_request.priority > g_system_ctx.g_force_display_stack[insert_pos].priority) {
        insert_pos++;
    }
    
    // 将插入点之后的所有元素向后移动一位
    for (int i = g_system_ctx.g_force_display_stack_top; i >= insert_pos; i--) {
        g_system_ctx.g_force_display_stack[i + 1] = g_system_ctx.g_force_display_stack[i];
    }
    
    // 插入新元素
    g_system_ctx.g_force_display_stack[insert_pos] = new_request;
    g_system_ctx.g_force_display_stack_top++;

    SYS_DEBUG("Inserted request from 0x%X at stack position %d. New stack top: %d.", new_request.requester_id, insert_pos, g_system_ctx.g_force_display_stack_top);
}

/**
 * @brief 【新增】根据请求者ID从强制栈中移除一个请求
 * 
 * @param requester_id 发起请求的服务ID
 */
static void remove_from_force_stack(service_id_t requester_id) {
    int found_index = -1;

    // 查找要移除的元素
    for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
        if (g_system_ctx.g_force_display_stack[i].requester_id == requester_id) {
            found_index = i;
            break;
        }
    }
    
    if (found_index != -1) {
        SYS_DEBUG("Removing request from service 0x%X.", requester_id);
        // 将后续元素向前移动，覆盖被删除的元素
        for (int i = found_index; i < g_system_ctx.g_force_display_stack_top; i++) {
            g_system_ctx.g_force_display_stack[i] = g_system_ctx.g_force_display_stack[i + 1];
        }
        // 清理最后一个元素（现在是多余的）
        memset(&g_system_ctx.g_force_display_stack[g_system_ctx.g_force_display_stack_top], 0, sizeof(force_stack_item_t));
        g_system_ctx.g_force_display_stack_top--;
    } else {
        SYS_INFO("Attempted to release a force request that was not found (requester: 0x%X).", requester_id);
    }
}


// --- START OF FILE service_system.c (Implementation) ---
// ... (includes, 变量定义等) ...

/**
 * @brief 【实现】向UI广播当前所有强制请求的列表及其状态
 */
static void broadcast_forced_stack_state(void) {
    SYS_DEBUG("=== broadcast_forced_stack_state ENTRY ===");
    SYS_DEBUG("Force stack top: %d", g_system_ctx.g_force_display_stack_top);
    
    // 1. 分配一个专门的 payload
    // 注意：这个 payload 应该有自己的 alloc/free/copy 函数，或者使用通用内存

    if (g_system_ctx.g_force_display_stack_top < 0) {
        SYS_INFO("No forced display requests to broadcast.");
        SYS_DEBUG("=== broadcast_forced_stack_state EXIT (empty stack) ===");
        return; // 如果栈为空，则不需要广播
    }

    SYS_DEBUG("Allocating payload for forced stack broadcast...");
    forced_stack_changed_payload_t* payload =  (forced_stack_changed_payload_t*)infotainment_malloc(sizeof(forced_stack_changed_payload_t));

    if (!payload) {
        SYS_ERROR("Failed to allocate payload for forced stack broadcast.");
        SYS_DEBUG("=== broadcast_forced_stack_state EXIT (malloc failed) ===");
        return;
    }
    SYS_DEBUG("Payload allocated successfully: %p", payload);

    // 2. 填充 payload 内容
    payload->count = 0;
    SYS_DEBUG("Starting to fill payload with stack items...");

    for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
        if (payload->count < MAX_FORCE_STACK_DEPTH) {
            SYS_DEBUG("  [%d] Adding stack item: widget_id=%d, requester=0x%X, minimized=%s", 
                      i, g_system_ctx.g_force_display_stack[i].intent.widget_id,
                      g_system_ctx.g_force_display_stack[i].requester_id,
                      g_system_ctx.g_force_display_stack[i].is_minimized ? "true" : "false");
            
            payload->stack[payload->count].intent = g_system_ctx.g_force_display_stack[i].intent;
            payload->stack[payload->count].requester_id = g_system_ctx.g_force_display_stack[i].requester_id;
            payload->stack[payload->count].is_minimized = g_system_ctx.g_force_display_stack[i].is_minimized;
            payload->count++;
        } else {
            SYS_ERROR("Payload stack overflow! Unable to add item %d", i);
            break;
        }
    }
    
    SYS_DEBUG("Payload filled with %d items", payload->count);

    // 3. 发送广播消息给UI
    SYS_DEBUG("Broadcasting forced stack state. Count: %d", payload->count);
    SYS_DEBUG("Sending SYSTEM_EVT_FORCED_STACK_CHANGED message to UI");
    bus_post_message(
        SRC_SYSTEM_SERVICE,
        ADDRESS_UI,
        MSG_PRIO_LOW, // 这是一个低优先级的状态更新事件
        SYSTEM_EVT_FORCED_STACK_CHANGED,
        payload,
        SRC_UNDEFINED, // 假设这个 payload 属于 system service
        NULL
    );
    SYS_DEBUG("=== broadcast_forced_stack_state EXIT (success) ===");
}

/**
 * @brief 【核心修改】获取指定“媒体意图”的优先级
 */
static int get_media_intent_priority(media_intent_t intent) {
    switch (intent) {
        // 安全和通信 > 紧急通知 > 用户娱乐 > 系统声音
        case MEDIA_INTENT_PHONE_CALL:           return 100;
        case MEDIA_INTENT_REVERSE_BEEP:         return 95;
        case MEDIA_INTENT_ADAS_ALERT:           return 90;
        case MEDIA_INTENT_TRAFFIC_ANNOUNCEMENT: return 80;
        case MEDIA_INTENT_NAVIGATION_PROMPT:    return 70;
        case MEDIA_INTENT_VR_SESSION:           return 60;
        case MEDIA_INTENT_USER_MEDIA_PLAYBACK:  return 20;
        case MEDIA_INTENT_SYSTEM_SOUND:         return 10;
        default:                                return 0;
    }
}


// --- START OF FILE service_system.c (Function Implementation) ---

#include "service_system.h"
#include "ui_screens.h" // 需要查询布局信息
#include "bus_router.h" // 只是为了上下文完整
#include <string.h>

// ... (其他 service_system.c 的内容) ...

/**
 * @brief 【实现】根据一个核心的 widget 意图，查找最适合的屏幕布局ID
 * 
 * 这个函数用于响应用户的导航请求，当用户想切换到一个包含特定功能的新界面时调用。
 * 
 * @param widget_id 用户请求的核心控件ID，例如 MANGER_ID_NAVIGATION_MAP
 * @return 找到的最佳 ui_screen_id_t，如果找不到任何能显示该控件的布局，则返回 UI_SCREEN_NONE。
 */
static ui_screen_id_t find_best_layout_for_widget(ui_manger_id_t widget_id) {
    SYS_DEBUG("Finding best layout for widget_id: %d", widget_id);

    // 1. 从 ui_screens 模块获取所有已定义的布局
    int num_profiles;
    const layout_profile_t* all_profiles = ui_screen_get_all_profiles(&num_profiles);
    if (!all_profiles || num_profiles == 0) {
        SYS_ERROR("No layout profiles are defined in ui_screens module.");
        return UI_SCREEN_NONE;
    }

    // 2. 初始化用于寻找“最佳”匹配的变量
    const layout_profile_t* best_match_profile = NULL;
    int best_score = 999; // 我们希望得分越低越好（控件数量越少）

    // 3. 遍历所有已定义的布局
    for (int i = 0; i < num_profiles; i++) {
        const layout_profile_t* current_profile = &all_profiles[i];
        
        // a. 检查当前布局是否包含了用户请求的 widget
        bool contains_target_widget = false;
        for (int j = 0; j < current_profile->widget_count; j++) {
            if (current_profile->widgets[j].type == widget_id) {
                contains_target_widget = true;
                break;
            }
        }
        
        // 如果不包含，则直接跳过这个布局
        if (!contains_target_widget) {
            continue;
        }

        // b. 如果包含，则它是一个候选布局。我们为其计算一个“评分”。
        //    这里的评分策略是：控件总数越少，布局越专用，得分越低（越好）。
        int current_score = current_profile->widget_count;

        SYS_DEBUG("  -> Candidate layout found: '%s' (ID:%d) with %d widgets.", 
                  current_profile->description, current_profile->profile_id, current_score);

        // c. 与当前已找到的最佳匹配进行比较
        if (current_score < best_score) {
            best_score = current_score;
            best_match_profile = current_profile;
            SYS_DEBUG("     -> New best match found!");
        }
    }

    // 4. 返回最佳匹配的结果
    if (best_match_profile) {
        SYS_INFO("Best layout for widget %d is '%s' (ID:%d).", 
                 widget_id, best_match_profile->description, best_match_profile->profile_id);
        return best_match_profile->profile_id;
    } else {
        SYS_ERROR("No layout profile found that contains widget_id: %d.", widget_id);
        return UI_SCREEN_NONE; // 明确表示未找到
    }
}

/**
 * @brief qsort 的比较函数，用于按优先级降序排序
 */
static int compare_intents_by_priority(const void* a, const void* b) {
    // 这是一个通用的比较函数，可以用于比较任何包含 priority 字段的结构体
    // 这里我们假设它用于比较一个临时结构
    typedef struct { int priority; } PrioStruct;
    return ((const PrioStruct*)b)->priority - ((const PrioStruct*)a)->priority;
}

/**
 * @brief 智能决策引擎：根据当前所有意图选择最佳布局和内容源
 * @param[out] out_source_type 用于返回决策出的内容源
 * @return 决策出的最佳屏幕ID
 */
static ui_screen_id_t decide_best_layout(ui_source_type_t* out_source_type) {
    *out_source_type = UI_SOURCE_TYPE_NONE; // 默认

    // 1. 收集所有当前活跃的（未被最小化的）意图
    display_intent_t active_intents[MAX_FORCE_STACK_DEPTH + MAX_USER_CONTENT_REQUESTS];
    int priorities[MAX_FORCE_STACK_DEPTH + MAX_USER_CONTENT_REQUESTS];
    int active_count = 0;
    
    // a. 从强制栈收集
    for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
        if (!g_system_ctx.g_force_display_stack[i].is_minimized) {
            active_intents[active_count] = g_system_ctx.g_force_display_stack[i].intent;
            priorities[active_count] = g_system_ctx.g_force_display_stack[i].priority;
            active_count++;
        }
    }
    
    // b. 如果没有活跃的强制意图，则从用户层收集
    if (active_count == 0) {
        // 首先，用户所在的屏幕本身就是一个隐式的“显示意图”
        // 我们需要找到一个能代表这个屏幕的控件
        // 这是一个复杂点，为简化，我们假设用户导航只改变内容源
        for (int i = 0; i < g_system_ctx.g_user_content_requests_count; i++) {
            active_intents[active_count] = g_system_ctx.g_user_content_requests[i].intent;
            priorities[active_count] = g_system_ctx.g_user_content_requests[i].priority;
            active_count++;
        }
    }

    if (active_count == 0) {
        // 没有任何活跃意图，返回用户当前屏幕和默认内容源
        *out_source_type = g_system_ctx.g_user_current_source;
        return g_system_ctx.g_user_current_screen;
    }

    // 2. 将活跃意图按优先级从高到低排序 (需要一个临时结构体来排序)
    typedef struct { display_intent_t intent; int priority; } TempIntent;
    TempIntent sorted_intents[active_count];
    for(int i=0; i<active_count; ++i) {
        sorted_intents[i].intent = active_intents[i];
        sorted_intents[i].priority = priorities[i];
    }
    qsort(sorted_intents, active_count, sizeof(TempIntent), compare_intents_by_priority);

    // 3. 迭代尝试匹配布局（从满足所有意图开始，逐步降级）
    int num_profiles;
    const layout_profile_t* all_profiles = ui_screen_get_all_profiles(&num_profiles);

    for (int num_to_satisfy = active_count; num_to_satisfy > 0; num_to_satisfy--) {
        ui_widget_mask_t required_widgets = 0;
        for (int i = 0; i < num_to_satisfy; i++) {
            required_widgets |= ((ui_widget_mask_t)1 << sorted_intents[i].intent.widget_id);
        }

        const layout_profile_t* best_match = NULL;
        int best_score = 999;

        for (int i = 0; i < num_profiles; i++) {
            ui_widget_mask_t layout_widgets = ui_screen_get_widgets_by_id(all_profiles[i].profile_id);
            if ((layout_widgets & required_widgets) == required_widgets) {
                int score = 0;
                for(int k=0; k<MANGER_ID_MAX; ++k) if((layout_widgets >> k)&1) score++;
                if (score < best_score) {
                    best_score = score;
                    best_match = &all_profiles[i];
                }
            }
        }
        
        if (best_match) {
            // 找到了最佳布局，现在确定这个布局下的 source_type
            int max_source_prio = -1;
            for (int i = 0; i < num_to_satisfy; i++) {
                if (sorted_intents[i].intent.widget_id == MANGER_ID_SOURCE && sorted_intents[i].priority > max_source_prio) {
                    max_source_prio = sorted_intents[i].priority;
                    *out_source_type = sorted_intents[i].intent.source_type;
                }
            }
            return best_match->profile_id;
        }
    }

    SYS_ERROR("No layout found even for the highest priority intent!");
    return g_system_ctx.g_user_current_screen; // 降级到用户当前屏幕
}
// --- START OF FILE service_system.c (Re-evaluation Function) ---

/**
 * @brief 核心仲裁与执行函数
 */
static void re_evaluate_and_switch_display(void) {
    SYS_DEBUG("=== re_evaluate_and_switch_display ENTRY ===");
    SYS_DEBUG("Current system state: screen_id=%d, source_type=%d", 
              g_system_ctx.system_data.screen_id, g_system_ctx.system_data.initial_source_type);
    SYS_DEBUG("Force stack top: %d, User current screen: %d", 
              g_system_ctx.g_force_display_stack_top, g_system_ctx.g_user_current_screen);
    
    switch_to_screen_payload_t new_state;

    // 1. 调用决策引擎，计算出理想的UI状态
    SYS_DEBUG("Calling decide_best_layout...");
    new_state.screen_id = decide_best_layout(&new_state.initial_source_type);
    SYS_DEBUG("Decision result: screen_id=%d, source_type=%d", 
              new_state.screen_id, new_state.initial_source_type);

    // 2. 只有在最终决策与当前UI显示不一致时才发送指令
    if (g_system_ctx.system_data.screen_id != new_state.screen_id ||
        g_system_ctx.system_data.initial_source_type != new_state.initial_source_type) {
        
        SYS_INFO("Policy Decision: Switching UI to screen %d with source_type %d.",
                 new_state.screen_id, new_state.initial_source_type);
        
        // 准备 payload
        system_state_payload_t* payload_to_send = service_system_payload_alloc();
        if (payload_to_send) {
            payload_to_send->screen_id = new_state.screen_id; // 拷贝决策结果
            payload_to_send->initial_source_type = new_state.initial_source_type; // 拷贝决策结果
            
            // 发送切换指令
            if(g_boot_notification_sent == false)
            {
                bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH,
                            SYSTEM_EVT_SYSTEM_OPEN_UI, 
                            payload_to_send, 
                            SRC_SYSTEM_SERVICE, 
                            NULL);
                g_boot_notification_sent = true; // 标记已发送
            }
            else
            {
                bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH,
                            SYSTEM_EVT_SYSTEM_SWITCH_TO_SCREEN, 
                            payload_to_send, 
                            SRC_SYSTEM_SERVICE, 
                            NULL);
            }
            
            // 更新内部记录的“已下发”状态
            g_system_ctx.system_data.screen_id = new_state.screen_id;
            g_system_ctx.system_data.initial_source_type = new_state.initial_source_type;
        }
    } else {
        if(g_boot_notification_sent == false)
        {
             system_state_payload_t* payload_to_send = service_system_payload_alloc();
            if (payload_to_send) {
                payload_to_send->screen_id = new_state.screen_id; // 拷贝决策结果
                payload_to_send->initial_source_type = new_state.initial_source_type; // 拷贝决策结果
                bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH,
                            SYSTEM_EVT_SYSTEM_OPEN_UI, 
                            payload_to_send, 
                            SRC_SYSTEM_SERVICE, 
                            NULL);
                g_boot_notification_sent = true; // 标记已发送
            }
            return;
        }
        SYS_DEBUG("Policy evaluation resulted in no change to the UI.");
    }
}


void service_system_user_navigate_to_widget(request_display_intent_payload_t* req)
{
    if (!req) { SYS_ERROR("Received USER_NAVIGATE_TO with NULL payload."); return; }

    SYS_INFO("Handling USER_NAVIGATE_TO intent: widget=%d, source_type=%d", 
            req->intent.widget_id, req->intent.source_type);

    // --- 1. 检查当前屏幕是否已包含该 widget ---
    if (currently_displayed_layout_contains_widget(req->intent.widget_id)) {
        
        // --- 2. 如果存在，则只改变 source_type ---
        SYS_INFO("Widget %d already exists in current screen %d. Updating source type.", 
                req->intent.widget_id, g_system_ctx.g_user_current_screen);
        
        // 找到并更新用户层的内容请求
        bool updated = false;
        for (int i = 0; i < g_system_ctx.g_user_content_requests_count; ++i) {
            if (g_system_ctx.g_user_content_requests[i].intent.widget_id == req->intent.widget_id) {
                g_system_ctx.g_user_content_requests[i].intent.source_type = req->intent.source_type;
                g_system_ctx.g_user_content_requests[i].priority = PRIORITY_USER_NORMAL;//get_media_intent_priority(MEDIA_INTENT_USER_MEDIA_PLAYBACK);
                g_system_ctx.g_user_content_requests[i].requester_id = SRC_UI; // 标记为用户最新请求
                updated = true;
                initialize_user_layer_from_profile(g_system_ctx.system_data.screen_id,req->intent.source_type);
                break;
            }
        }
        if (!updated) { // 如果之前没有这个内容请求（例如，source控件之前是空的）
            // add_to_user_content_requests(...);
        }

    } else {
        
        // --- . 如果不存在，则需要切换布局 ---
        SYS_INFO("Widget %d does not exist in current screen %d. Attempting to switch layout.",
                req->intent.widget_id, g_system_ctx.g_user_current_screen);

        // --- 1. 安全检查：检查是否有不可最小化的强制屏幕 ---
        if (g_system_ctx.g_force_display_stack_top >= 0) {
            // 栈顶的请求就是当前最高优先级的强制请求
            force_stack_item_t* current_top_request = &g_system_ctx.g_force_display_stack[g_system_ctx.g_force_display_stack_top];
            
            if (!current_top_request->is_minimizable) {
                SYS_INFO("User navigation blocked by non-minimizable screen (widget ID: %x, Requester: 0x%X).",
                        current_top_request->intent.widget_id, current_top_request->requester_id);
                
                // (可选) 可以给UI发送一个NACK，让UI弹出一个简短的提示，如“倒车时无法操作”
                // bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, ..., SYSTEM_NACK_ACTION_DENIED, ...);
                
                return; // 明确拒绝并终止导航流程
            }
        }

            // --- 2. 批准导航。最小化所有可最小化的强制请求 ---
        SYS_DEBUG("User navigation approved. Minimizing all minimizable forced intents.");
        bool stack_changed = false;
        for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
            // 只有当一个请求是活跃的(未被最小化)且是可最小化的，才去最小化它
            if (!g_system_ctx.g_force_display_stack[i].is_minimized && g_system_ctx.g_force_display_stack[i].is_minimizable) {
                g_system_ctx.g_force_display_stack[i].is_minimized = true;
                stack_changed = true;
                SYS_DEBUG("  -> Minimized request from service 0x%X for widget id %d.",
                        g_system_ctx.g_force_display_stack[i].requester_id, g_system_ctx.g_force_display_stack[i].intent.widget_id);
            }
        }

        
        ui_screen_id_t new_screen_id = find_best_layout_for_widget(req->intent.widget_id);
        if (new_screen_id == UI_SCREEN_NONE) {
            SYS_ERROR("No suitable layout found for widget %d. Navigation aborted.", req->intent.widget_id);
            return; // 没有找到合适的布局，终止导航
        }
        // 使用新布局来重新初始化用户层
        initialize_user_layer_from_profile(new_screen_id, req->intent.source_type);
    }
    
    // --- 4. 无论哪种情况，最后都重新评估并通知UI ---
    re_evaluate_and_switch_display();
    broadcast_forced_stack_state(); // 如果最小化了强制请求，需要通知UI
}


void service_system_user_minimize_forced(void)
{
    SYS_INFO("Handling user request to minimize the current forced screen.");

    // 1. 检查强制栈是否为空
    if (g_system_ctx.g_force_display_stack_top < 0) {
        SYS_DEBUG("No forced screen is active, nothing to minimize.");
        return; // 栈为空，无需操作
    }
    
    // 2. 获取栈顶的、当前正在显示的强制请求
    force_stack_item_t* top_request = &g_system_ctx.g_force_display_stack[g_system_ctx.g_force_display_stack_top];

    // 3. 检查它是否是可最小化的
    if (top_request->is_minimizable) {
        // 4. 执行最小化操作
        if (!top_request->is_minimized) {
            top_request->is_minimized = true;
            SYS_INFO("Screen for requester 0x%X (prio:%d) has been minimized.",
                        top_request->requester_id, top_request->priority);

            // 5. 重新评估显示策略，这会导致UI切换到下一个可用屏幕
            re_evaluate_and_switch_display();

            // 6. 广播强制栈状态变化，通知UI更新状态栏
            broadcast_forced_stack_state();
        } else {
            SYS_DEBUG("Screen for requester 0x%X was already minimized.", top_request->requester_id);
        }
    } else {
        SYS_INFO("Attempt to minimize a non-minimizable screen (requester: 0x%X) was blocked.",
                    top_request->requester_id);
        // (可选) 发送NACK给UI，让其播放一个提示音或显示一个toast
    }
}


void service_system_user_restore_forced(user_restore_payload_t* req)
{
    if (!req) {
        SYS_ERROR("Received USER_RESTORE_FORCED with NULL payload.");
        return;
    }

    SYS_INFO("Handling user request to restore screen for requester 0x%X.", req->requester_id);
    
    bool found_and_restored = false;

    // 1. 遍历整个强制栈，找到对应的请求
    for (int i = 0; i <= g_system_ctx.g_force_display_stack_top; i++) {
        if (g_system_ctx.g_force_display_stack[i].requester_id == req->requester_id) {
            
            // 2. 检查它是否真的处于最小化状态
            if (g_system_ctx.g_force_display_stack[i].is_minimized) {
                // 3. 执行恢复操作
                g_system_ctx.g_force_display_stack[i].is_minimized = false;
                found_and_restored = true;
                SYS_INFO("Request from service 0x%X has been restored to active.", req->requester_id);
            } else {
                SYS_DEBUG("Request from service 0x%X was already active.", req->requester_id);
            }
            
            // 4. (可选) 如果需要，可以将此请求“提升”到栈顶，确保它立即显示
            //    但这会破坏原有的优先级顺序。更好的做法是依赖 re_evaluate。
            
            break; // 找到后即可退出循环
        }
    }

    if (found_and_restored) {
        // 5. 重新评估显示策略，这会导致UI切换回被恢复的屏幕（如果它是最高优先级的话）
        re_evaluate_and_switch_display();

        // 6. 广播强制栈状态变化，通知UI更新状态栏
        broadcast_forced_stack_state();
    } else {
        SYS_ERROR("Could not find a minimizable request for requester 0x%X to restore.", req->requester_id);
    }
}




void service_system_handle_msg(app_message_t* received_msg)
{
    unsigned char err = 0;
    ref_counted_payload_t* wrapper = NULL;

    if (!received_msg) {
        SYS_DEBUG("No message received, continuing...");
        return; // 没有消息
    }
    SYS_DEBUG("收到消息 (来源:0x%x, 命令:0x%x)", received_msg->source, received_msg->command);

    if (received_msg->command == CMD_PING_REQUEST || received_msg->command == CMD_PING_BROADCAST) {
        service_handle_ping_request(received_msg);
        return;
    }
    else if (received_msg->command == CMD_PONG_RESPONSE) {
        service_handle_pong_response(received_msg);
        return;
    }

    switch(received_msg->source) {
        case SRC_UI: {
            wrapper = (ref_counted_payload_t*)received_msg->payload;
            if(received_msg->command == SYSTEM_CMD_USER_NAVIGATE_TO)
            {
                service_system_user_navigate_to_widget((request_display_intent_payload_t *)wrapper->ptr);
            }
            else if(received_msg->command == SYSTEM_CMD_USER_MINIMIZE_FORCED)
            {
                service_system_user_minimize_forced();
            }
            else if(received_msg->command == SYSTEM_CMD_USER_RESTORE_FORCED)
            {
                service_system_user_restore_forced((user_restore_payload_t *)wrapper->ptr);
            }
           
            break;
        }
        case SRC_RADIO_SERVICE: {
            // 处理来自收音机服务的消息
            wrapper = (ref_counted_payload_t*)received_msg->payload;

            if(received_msg->command == RADIO_EVT_READY)
            {
                // 收音机服务准备就绪，设置状态并检查是否所有服务都已就绪
                g_ready_services_mask |= ADDRESS_RADIO_SERVICE;
                SYS_INFO("Radio service is ready");
                check_all_services_ready_and_notify_ui();
            }

            else if(received_msg->command == SYSTEM_CMD_FORCE_REQUEST_DISPLAY)
            {
                force_request_payload_t* req = wrapper->ptr;
                if (req) {
                    force_stack_item_t new_request = {
                    .intent = req->intent, // <-- 保存的是意图
                    .requester_id = received_msg->source,
                    .priority = get_media_intent_priority(req->media_intent),
                    .is_minimizable = req->is_minimizable,
                    .is_minimized = false
                };
                insert_to_force_stack(new_request); // 插入到强制栈
                re_evaluate_and_switch_display();
                broadcast_forced_stack_state();
                }
            }
            else if(received_msg->command == SYSTEM_CMD_FORCE_RELEASE_DISPLAY)
            {
                remove_from_force_stack(received_msg->source);
                re_evaluate_and_switch_display();
                broadcast_forced_stack_state();
            }

        }
        break;

        case SRC_TIMER_SERVICE: {
            // 处理来自定时器服务的消息
            wrapper = (ref_counted_payload_t*)received_msg->payload;

            if(received_msg->command == CMD_PING_TEST_START)
            {
                SYS_DEBUG("Received ping request from timer service");
                start_ping_pong_test(SRC_SYSTEM_SERVICE, SRC_UI, "System->UI ping测试",1); // 发送ping请求
            }
            // 处理内部定时器超时消息
            else if (received_msg->command >= SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT &&
                     received_msg->command <= SYSTEM_CMD_INTERNAL_UI_READY_TIMEOUT) {
                system_handle_internal_timer_timeout(received_msg->command);
            }
        }
        break;

        case SRC_SYSTEM_SERVICE: {
            // 处理来自自己的内部消息（定时器超时等）
            if (received_msg->command >= SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT &&
                received_msg->command <= SYSTEM_CMD_INTERNAL_UI_READY_TIMEOUT) {
                system_handle_internal_timer_timeout(received_msg->command);
            }
        }
        break;

        default: {
            SYS_ERROR("Received message from unknown source 0x%x", received_msg->source);
            break;
        }

    }
  

}

//获取服务注册表共享数据
static void system_get_service_reg_data(void)
{
    g_system_ctx.system_reg_para = dsk_reg_get_para_by_app(REG_APP_SYSTEM);
    g_system_ctx.init_reg_para = dsk_reg_get_para_by_app(REG_APP_INIT);
    g_system_ctx.root_reg_para = dsk_reg_get_para_by_app(REG_APP_ROOT);
}


// 线程主函数
static void system_thread_entry(void* p_arg) {
    unsigned char err = 0;

    check_all_services_ready_and_notify_ui();
    if(g_boot_notification_sent == false)
    {
        uint32_t required_timeout = get_dependencies_timeout_for_screen(g_system_ctx.system_data.screen_id);
        // 启动系统定时器
        SYS_INFO("Starting system timers...");
        system_timer_start(SYSTEM_TIMER_STARTUP_DELAY, required_timeout, TIMER_TYPE_ONE_SHOT,
                        SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT, NULL);
    }
    // 可以发送系统就绪事件给UI
    bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH,
                    SYSTEM_EVT_READY, NULL, SRC_UNDEFINED, NULL);
    #if 0
    // 1. 启动系统启动超时定时器 (30秒)
    system_timer_start(SYSTEM_TIMER_STARTUP_DELAY, 30000, TIMER_TYPE_ONE_SHOT,
                      SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT, NULL);

    // 2. 启动服务状态检查定时器 (每5秒检查一次)
    system_timer_start(SYSTEM_TIMER_SERVICE_CHECK, 5000, TIMER_TYPE_PERIODIC,
                      SYSTEM_CMD_INTERNAL_SERVICE_CHECK, NULL);

    // 3. 启动系统看门狗定时器 (每60秒)
    system_timer_start(SYSTEM_TIMER_WATCHDOG, 60000, TIMER_TYPE_PERIODIC,
                      SYSTEM_CMD_INTERNAL_WATCHDOG_TIMEOUT, NULL);

    // 4. 启动UI就绪检查定时器 (10秒超时)
    system_timer_start(SYSTEM_TIMER_UI_READY_CHECK, 10000, TIMER_TYPE_ONE_SHOT,
                      SYSTEM_CMD_INTERNAL_UI_READY_TIMEOUT, NULL);

    //start_ping_pong_test(SRC_SYSTEM_SERVICE, SRC_UI, "System->UI ping测试",1024); // 发送ping请求
    #endif

        // 1. 启动系统启动超时定时器 (30秒)
   // system_timer_start(SYSTEM_TIMER_PING_TEST, 1000, TIMER_TYPE_PERIODIC,
                  //    SYSTEM_CMD_INTERNAL_PING_TEST_TIMEOUT, NULL);

    while (1) {
        if (infotainment_thread_TDelReq(EXEC_prioself))
        {
            goto exit;
        }
        
         uint32_t timeout = 0;//calculate_dynamic_timeout();
         app_message_t* received_msg = (app_message_t*)infotainment_queue_pend(service_system_queue_handle, timeout, &err);
         if(received_msg && err==OS_NO_ERR) {
            //printf("2=0x%2x\n", (int)received_msg);
            service_system_handle_msg(received_msg);

              // 释放消息内存
            if (received_msg->payload) {
               // SYS_DEBUG("received_msg->payload=%p,ref_count=%d", received_msg->payload,((ref_counted_payload_t*)received_msg->payload)->ref_count);
               //printf("1=0x%2x\n", (int)received_msg);
                free_ref_counted_payload((ref_counted_payload_t*)received_msg->payload);
            }
            infotainment_free(received_msg);
            received_msg = NULL;
            SYS_DEBUG("Message processed and memory freed");
         }

    }

     exit:
        infotainment_thread_delete(EXEC_prioself);
}


void service_system_init(void) {
    unsigned char err = 0;
    SYS_INFO("Initializing system service...");

    // 初始化系统上下文
    memset(&g_system_ctx, 0, sizeof(system_service_context_t));
    for (int i = 0; i < SYSTEM_TIMER_TYPE_COUNT; i++) {
        g_system_ctx.active_timers[i].type = SYSTEM_TIMER_NONE;
        g_system_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
    }
    g_system_ctx.startup_time = infotainment_get_ticks();
    g_system_ctx.system_data.screen_id = UI_SCREEN_HOME;    
    g_system_ctx.system_data.initial_source_type = UI_SOURCE_TYPE_FM;

    g_system_ctx.g_force_display_stack_top = -1;
    g_system_ctx.g_user_current_screen = UI_SCREEN_HOME;
    g_system_ctx.g_user_current_source = UI_SOURCE_TYPE_FM;
    g_system_ctx.g_user_content_requests_count = 0;

    // 申请信号量
    service_system_semaphore = infotainment_sem_create(1);
    if (!service_system_semaphore) {
        SYS_ERROR("Failed to create semaphore");
        return;
    }

    SYS_INFO("Thread created with handle %d", service_system_thread_handle);
    // 创建消息队列
    service_system_queue_handle = infotainment_queue_create(SYSTEM_QUEUE_SIZE);
    if (!service_system_queue_handle) {
        SYS_ERROR("Failed to create message queue");
        infotainment_thread_delete(service_system_thread_handle);
        return;
    }
    SYS_INFO("Message queue created with handle %p", service_system_queue_handle);
    // 注册服务到路由器
    if (bus_router_register_service(SRC_SYSTEM_SERVICE, ADDRESS_SYSTEM_SERVICE, service_system_queue_handle,"System service",service_system_payload_free,service_system_payload_copy) < 0) {
        SYS_ERROR("Failed to register service with bus router");
        infotainment_queue_delete(service_system_queue_handle, &err);
        infotainment_thread_delete(service_system_thread_handle);
        return;
    }

    system_get_service_reg_data();
    service_system_update_payload_data();
    initialize_user_layer_from_profile(g_system_ctx.system_data.screen_id, g_system_ctx.system_data.initial_source_type);
    g_ready_services_mask |= ADDRESS_SYSTEM_SERVICE;//系统服务就绪

    // 创建并启动线程
    service_system_thread_handle = infotainment_thread_create(system_thread_entry, NULL,TASK_PROI_LEVEL2,0x8000,"ServiceSystemThread");
    if(service_system_thread_handle < 0) {
        SYS_ERROR("Failed to create thread");
        infotainment_sem_delete(service_system_semaphore, &err);
        service_system_semaphore = NULL;
        return;
    }
}


void service_system_exit(void) {
    unsigned char err = 0;
    SYS_INFO("Exiting system service...");

    // 停止所有定时器
    system_timer_stop_all();

    bus_router_unregister_service(SRC_SYSTEM_SERVICE); // 注销服务

    // 释放线程句柄
    if (service_system_thread_handle > 0) {            
        if(service_system_queue_handle)
            infotainment_queue_post(service_system_queue_handle, NULL);
        while(infotainment_thread_delete_req(service_system_thread_handle)!=OS_TASK_NOT_EXIST)
        {        
            if(service_system_queue_handle)
                infotainment_queue_post(service_system_queue_handle, NULL);
            infotainment_thread_sleep(10);
        }
        service_system_thread_handle = 0;
    }

    // 释放消息队列
    if (service_system_queue_handle) {
        unsigned char err = 0;
        void* msg = NULL;
        if (service_system_semaphore)
            infotainment_sem_wait(service_system_semaphore, 0, &err);
        // 不断从队列取出消息并释放，直到队列为空
        while ((msg = infotainment_queue_get(service_system_queue_handle, &err)) && err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg && app_msg->payload) {
                free_ref_counted_payload((ref_counted_payload_t*)app_msg->payload);
            }
            if(app_msg)
                infotainment_free(app_msg);

        }
        infotainment_queue_delete(service_system_queue_handle, &err);
        service_system_queue_handle = NULL;
        if (service_system_semaphore)
            infotainment_sem_post(service_system_semaphore);
    }
    // 删除信号量
    if (service_system_semaphore) {
        infotainment_sem_delete(service_system_semaphore, &err);
        service_system_semaphore = NULL;
    }
    SYS_INFO("System service cleanup complete");
}

static const char* system_state_to_string(system_state_t state);
/**
 * @brief 进入新状态时要执行的动作
 */
static void system_enter_state(system_state_t new_state) {
    SYS_INFO("Entering state: %s", system_state_to_string(new_state));
    if (new_state == g_system_ctx.current_state) {
        SYS_ERROR("Already in state: %s", system_state_to_string(new_state));
        return;
    }
    // 更新全局状态
    g_system_ctx.current_state = new_state;

    switch (new_state) {
        case SYSTEM_STATE_INIT:
            // 进入初始化状态时，启动一个从MCU获取内存的定时器
           
            break;

        case SYSTEM_STATE_IDLE:
            break;
  
        // ... 其他状态的进入动作 ...
        default:
            break;
    }
}




/**
 * @brief 退出旧状态时要执行的清理动作
 */
static void system_exit_state(system_state_t old_state) {
    SYS_INFO("Exiting state: %s", system_state_to_string(old_state));
    
    switch (old_state) {
        case SYSTEM_STATE_INIT:
            // 退出初始化状态时，停止从MCU获取内存的定时器

            break;
        case SYSTEM_STATE_IDLE:
            break;
        
        // ... 其他状态的退出清理动作 ...
        default:
            break;
    }
}

/**
 * @brief 将状态枚举转换为字符串，便于调试
 */
static const char* system_state_to_string(system_state_t state) {
    switch (state) {
        case SYSTEM_STATE_IDLE: return "IDLE";
        case SYSTEM_STATE_INIT: return "INIT";
        default: return "UNKNOWN";
    }
}





// ================================
// 定时器管理函数实现
// ================================

/**
 * @brief 启动一个内部定时器，并自动管理其ID
 * @param timer_type 我们定义的内部定时器类型
 * @param duration_ms 定时器持续时间(毫秒)
 * @param type 定时器类型(单次/周期)
 * @param target_command 超时时发送的命令
 * @param user_data 用户数据
 */
static void system_timer_start(system_internal_timer_type_t timer_type, uint32_t duration_ms,
                              timer_type_t type, unsigned int target_command, void* user_data) {
    // 在启动新定时器前，先确保同类型的旧定时器已被停止
    system_timer_stop(timer_type);

    uint32_t new_id = timer_start(
        duration_ms,
        type,
        ADDRESS_SYSTEM_SERVICE,
        target_command,
        user_data,
        user_data ? SRC_SYSTEM_SERVICE : SRC_UNDEFINED // 假设 payload 都是 system 自己创建的
    );

    if (new_id != TIMER_SERVICE_INVALID_ID) {
        // 成功启动，将ID保存到我们的管理数组中
        for (int i = 0; i < SYSTEM_TIMER_TYPE_COUNT; i++) {
            if (g_system_ctx.active_timers[i].type == SYSTEM_TIMER_NONE) {
                g_system_ctx.active_timers[i].type = timer_type;
                g_system_ctx.active_timers[i].timer_id = new_id;
                SYS_DEBUG("Internal timer started: type=%d, id=%u", timer_type, new_id);
                return;
            }
        }
        // 如果数组满了（理论上不应该发生），停止刚刚创建的定时器
        SYS_ERROR("Active timers array is full! Stopping orphaned timer %u", new_id);
        timer_stop(new_id);
    } else {
        SYS_ERROR("Failed to start internal timer: type=%d", timer_type);
    }
}

/**
 * @brief 停止一个内部定时器
 * @param timer_type 我们定义的内部定时器类型
 */
static void system_timer_stop(system_internal_timer_type_t timer_type) {
    for (int i = 0; i < SYSTEM_TIMER_TYPE_COUNT; i++) {
        if (g_system_ctx.active_timers[i].type == timer_type) {
            if (g_system_ctx.active_timers[i].timer_id != TIMER_SERVICE_INVALID_ID) {
                timer_stop(g_system_ctx.active_timers[i].timer_id);
                SYS_DEBUG("Internal timer stopped: type=%d, id=%u", timer_type, g_system_ctx.active_timers[i].timer_id);
            }
            // 从管理数组中移除
            g_system_ctx.active_timers[i].type = SYSTEM_TIMER_NONE;
            g_system_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
            return;
        }
    }
    SYS_DEBUG("Timer type %d not found in active timers", timer_type);
}

/**
 * @brief 停止所有由本服务启动的定时器 (在退出时调用)
 */
static void system_timer_stop_all(void) {
    SYS_INFO("Stopping all system timers...");
    for (int i = 0; i < SYSTEM_TIMER_TYPE_COUNT; i++) {
        if (g_system_ctx.active_timers[i].type != SYSTEM_TIMER_NONE) {
            system_timer_stop(g_system_ctx.active_timers[i].type);
        }
    }
    SYS_INFO("All system timers stopped");
}

/**
 * @brief 处理内部定时器超时事件
 * @param command 超时命令
 */
static void system_handle_internal_timer_timeout(unsigned int command) {
    SYS_DEBUG("Handling internal timer timeout: command=0x%x", command);

    switch (command) {
        case SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT:
            if(g_boot_notification_sent == false)
            {
                SYS_INFO("Startup timeout reached - opening default UI");
                // 准备 payload
                system_state_payload_t* payload = service_system_payload_copy(&g_system_ctx.system_data);
                if (payload) {
                    bus_post_message(SRC_SYSTEM_SERVICE, ADDRESS_UI, MSG_PRIO_HIGH, SYSTEM_EVT_SYSTEM_OPEN_UI, payload, SRC_SYSTEM_SERVICE, NULL);
                    g_boot_notification_sent = true; // 标记已发送
                }

            }
            break;

        case SYSTEM_CMD_INTERNAL_SERVICE_CHECK:
            SYS_DEBUG("Performing periodic service check");
            // 定期检查各个服务的状态
            g_system_ctx.ping_test_count++;
            if (g_system_ctx.ping_test_count % 10 == 0) {
                // 每10次检查发送一次ping测试
                start_ping_pong_test(SRC_SYSTEM_SERVICE, SRC_UI, "System periodic ping", 1);
            }
            break;

        case SYSTEM_CMD_INTERNAL_PING_TEST_TIMEOUT:
            SYS_INFO("Ping test timeout - some services may be unresponsive");
            //start_ping_pong_test(SRC_SYSTEM_SERVICE, SRC_UI, "System periodic ping", 1);
            start_broadcast_test(SRC_SYSTEM_SERVICE);


            // ping测试超时处理
            break;

        case SYSTEM_CMD_INTERNAL_WATCHDOG_TIMEOUT:
            SYS_ERROR("System watchdog timeout - system may be unresponsive!");
            // 看门狗超时处理：可能需要重启某些服务或整个系统
            break;

        case SYSTEM_CMD_INTERNAL_SHUTDOWN_TIMEOUT:
            SYS_INFO("Shutdown timeout reached - forcing shutdown");
            // 强制关机处理
            break;

        case SYSTEM_CMD_INTERNAL_UI_READY_TIMEOUT:
            SYS_ERROR("UI ready timeout - UI may have failed to initialize");
            // UI就绪超时处理
            break;

        default:
            SYS_ERROR("Unknown internal timer command: 0x%x", command);
            break;
    }
}

// ================================
// 外部便利函数实现
// ================================

/**
 * @brief 启动系统启动超时定时器
 * @param timeout_ms 超时时间(毫秒)
 */
void service_system_start_startup_timer(uint32_t timeout_ms) {
    SYS_INFO("Starting startup timer: %u ms", timeout_ms);
    system_timer_start(SYSTEM_TIMER_STARTUP_DELAY, timeout_ms, TIMER_TYPE_ONE_SHOT,
                      SYSTEM_CMD_INTERNAL_STARTUP_TIMEOUT, NULL);
}

/**
 * @brief 启动系统看门狗定时器
 * @param interval_ms 看门狗间隔(毫秒)
 */
void service_system_start_watchdog_timer(uint32_t interval_ms) {
    SYS_INFO("Starting watchdog timer: %u ms interval", interval_ms);
    system_timer_start(SYSTEM_TIMER_WATCHDOG, interval_ms, TIMER_TYPE_PERIODIC,
                      SYSTEM_CMD_INTERNAL_WATCHDOG_TIMEOUT, NULL);
}

/**
 * @brief 停止所有系统定时器（外部接口）
 */
void service_system_stop_all_timers(void) {
    SYS_INFO("Stopping all system timers (external call)");
    system_timer_stop_all();
}

