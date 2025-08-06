#include <bus_router.h>
#include <elibs_stdio.h>    

// --- 调试功能相关定义 ---
static int g_bus_router_debug = 0;  // 调试开关，默认关闭
static int g_bus_router_info = 0;   // 信息开关，默认关闭
static int g_bus_router_ping_pong = 1;  // ping-pong日志开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define BR_DEBUG(fmt, ...) \
    do { \
        if (g_bus_router_debug) { \
            printf("[BusRouter-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define BR_INFO(fmt, ...) \
    do { \
        if (g_bus_router_info) { \
            printf("[BusRouter-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define BR_ERROR(fmt, ...) \
    printf("[BusRouter-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

// ping-pong 专用日志宏
#define BR_PING(fmt, ...) \
    do { \
        if (g_bus_router_ping_pong) { \
            printf("[BusRouter-PING:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)



static const char* address_mask_to_string(bus_router_address_t mask, char* buf, int buf_len);
// --- 2. 【核心】定义消息打印宏 ---
/**
 * @brief 打印一条 app_message_t 的完整详细信息
 * @param msg_ptr 指向 app_message_t 的指针
 * @param prefix  一个自定义的前缀字符串，用于标识打印场景 (e.g., "Posting", "Processing")
 */
#define BR_DUMP_MESSAGE(msg_ptr, prefix) \
    do { \
        if (g_bus_router_debug && (msg_ptr)) { \
            char dest_str[128]; \
            service_registry_entry_t src_info = {0}; \
            bus_router_get_service_info_by_id((msg_ptr)->source, &src_info); \
            service_registry_entry_t payload_src_info = {0}; \
            bus_router_get_service_info_by_id((msg_ptr)->payload_source, &payload_src_info); \
            \
            printf(" M E S S A G E  D U M P \n"); \
            printf("-> Location: %s:%d @ %s()\n", __FILE__, __LINE__, __FUNCTION__); \
            printf("-> Context:  %s\n", prefix); \
            printf("-> Msg Addr: %p\n", (msg_ptr)); \
            printf("   ├─ Source:      0x%X (%s)\n", (msg_ptr)->source, src_info.is_used ? src_info.name : "Unknown"); \
            printf("   ├─ Destination: 0x%llX (%s)\n", (msg_ptr)->destination, address_mask_to_string((msg_ptr)->destination, dest_str, sizeof(dest_str))); \
            printf("   ├─ Priority:    %d\n", (msg_ptr)->priority); \
            printf("   ├─ Command:     0x%X\n", (msg_ptr)->command); \
            printf("   └─ Payload Info:\n"); \
            if ((msg_ptr)->payload) { \
                ref_counted_payload_t* wrapper = (ref_counted_payload_t*)(msg_ptr)->payload; \
                printf("      ├─ Wrapper Addr:  %p\n", wrapper); \
                if(wrapper) { \
                    printf("      ├─ Real Payload:  %p\n", wrapper->ptr); \
                    printf("      ├─ Payload Src:   0x%X (%s)\n", (msg_ptr)->payload_source, payload_src_info.is_used ? payload_src_info.name : "Generic/Unknown"); \
                    printf("      └─ Ref Count:     %d\n", wrapper->ref_count); \
                } \
            } else { \
                printf("      └─ (No Payload)\n"); \
            } \
            printf("--------------------------------------------------\n"); \
        } \
    } while(0)


/**
 * @brief 【新增】专门用于打印 ref_counted_payload_t 包装器信息的宏
 * @param wrapper_ptr 指向 ref_counted_payload_t 的指针
 * @param prefix      一个自定义的前缀字符串
 */
#define BR_DUMP_WRAPPER(wrapper_ptr, prefix) \
    do { \
        if (g_bus_router_debug && (wrapper_ptr)) { \
            /* 注意：我们无法知道 payload_source，因此不打印它 */ \
            const service_registry_entry_t* free_func_owner = NULL; \
            /* 这是一个简单的反向查找，仅用于调试，可能不完全精确 */ \
            for(int i = 0; i < MAX_SERVICES; ++i) { \
                if(g_service_registry[i].is_used && g_service_registry[i].free_func == (wrapper_ptr)->free_func) { \
                    free_func_owner = &g_service_registry[i]; \
                    break; \
                } \
            } \
            \
            printf(" P A Y L O A D  W R A P P E R  D U M P \n"); \
            printf("-> Location: %s:%d @ %s()\n", __FILE__, __LINE__, __FUNCTION__); \
            printf("-> Context:  %s\n", prefix); \
            printf("-> Wrapper Addr: %p\n", (wrapper_ptr)); \
            printf("   ├─ Real Payload:  %p\n", (wrapper_ptr)->ptr); \
            printf("   ├─ Ref Count:     %d\n", (wrapper_ptr)->ref_count); \
            printf("   └─ Free Func:     %p (Owner: %s)\n", (wrapper_ptr)->free_func, free_func_owner ? free_func_owner->name : "Generic/Unknown"); \
            printf("--------------------------------------------------\n"); \
        } \
    } while(0)




// --- 模块私有变量 ---
static service_registry_entry_t g_service_registry[MAX_SERVICES]; // 服务注册表数组
static int g_service_count = 0; // 当前注册的服务数量
static void* g_router_queue_handle[MSG_PRIO_COUNT] = {0};
static int g_router_thread_handle = 0;
static int g_router_exiting = 0;
//static void *g_router_semaphore = NULL; // 路由器信号量，用于线程同步
static void* g_router_mutex = NULL;     // 用于保护缓冲区的互斥锁


// --- 全局共享的紧急指令通道 ---
static fast_lane_msg_t g_router_fast_lane_buffer[FAST_LANE_BUFFER_SIZE];
static volatile uint32_t g_head = 0;
static volatile uint32_t g_tail = 0;

static void* g_router_fast_lane_mutex = NULL;     // 用于保护缓冲区的互斥锁
void* g_router_fast_lane_semaphore = NULL; // 用于通知UI线程的信号量

// --- 2. 【新增】外部控制调试开关的函数 ---
void bus_router_set_debug(int enable) {
    g_bus_router_debug = enable;
    BR_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void bus_router_set_info(int enable) {
    g_bus_router_info = enable;
    BR_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

void bus_router_set_ping_pong_log(int enable) {
    g_bus_router_ping_pong = enable;
    printf("[BusRouter-PING] Ping-pong messages %s\n", enable ? "ENABLED" : "DISABLED");
}


// --- 1. 【新增】辅助函数：将地址掩码转换为字符串 ---
// 这个函数会将一个地址掩码（可能包含多个地址）转换成可读的字符串
// 例如: 0x5 (二进制 101) -> "UI | Radio"
static const char* address_mask_to_string(bus_router_address_t mask, char* buf, int buf_len) {
    if (mask == ADDRESS_BROADCAST) {
        snprintf(buf, buf_len, "BROADCAST");
        return buf;
    }
    
    buf[0] = '\0';
    bool first = true;
    
    // 加锁以安全地访问注册表
    // 注意：如果这个函数在锁内部被调用，就不需要再加锁了
    // 为简单起见，我们假设它可能在外部调用
    //unsigned char err;
   // infotainment_mutex_wait(g_router_mutex, 0, &err);
    
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used && (g_service_registry[i].address & mask)) {
            if (!first) {
                strncat(buf, " | ", buf_len - strlen(buf) - 1);
            }
            strncat(buf, g_service_registry[i].name, buf_len - strlen(buf) - 1);
            first = false;
        }
    }
    
    //infotainment_mutex_post(g_router_mutex);
    
    if (strlen(buf) == 0) {
        snprintf(buf, buf_len, "NONE(0x%llX)", mask);
    }
    
    return buf;
}



// 获取当前注册的服务数量
int bus_router_get_service_count(void) {
    return g_service_count;
}

// 列出所有已注册的服务信息
void bus_router_list_services(void) {
    unsigned char err = 0;
    
    //if(g_router_semaphore)
       // infotainment_sem_wait(g_router_semaphore, 0, &err);

    if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err);    
        
    BR_INFO("=== Bus Router Service Registry ===");
    BR_INFO("Total registered services: %d/%d", g_service_count, MAX_SERVICES);
    
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used) {
            BR_INFO("Slot[%d]: Service 0x%x, Address 0x%llx, Queue %p, Name %s", 
                   i, g_service_registry[i].id, 
                   g_service_registry[i].address, 
                   g_service_registry[i].queue_handle,
                   g_service_registry[i].name);
        }
    }
    BR_INFO("=====================================");
    
    //if(g_router_semaphore)
       // infotainment_sem_post(g_router_semaphore);
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex);
}

// 【新增】实现公共查询接口 (注意线程安全)
int bus_router_get_service_info_by_id(service_id_t id,service_registry_entry_t *temp_service) {
    // 这个函数返回的指针是临时的，不应长期持有
    // 更安全的做法是返回一个拷贝，但为了性能，返回指针也可以接受
    // 注意：外部调用者不应该修改返回的指针内容
    unsigned char err = 0;
    if(temp_service == NULL)
        return -1;
    
   // if(g_router_semaphore)
       // infotainment_sem_wait(g_router_semaphore, 0, &err);
    if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err);
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used && g_service_registry[i].id == id) {
            // 返回指向数组元素的指针
            memcpy(temp_service, &g_service_registry[i], sizeof(service_registry_entry_t));
            if(g_router_mutex)
                 infotainment_mutex_post(g_router_mutex);
            return 0;
        }
    }
    //if(g_router_semaphore)
        //infotainment_sem_post(g_router_semaphore);
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex);
    return -1;
}

int bus_router_get_service_info_by_address(bus_router_address_t address,service_registry_entry_t *temp_service) {
    unsigned char err = 0;
    
    if(temp_service == NULL)
        return -1;
    //if(g_router_semaphore)
        //infotainment_sem_wait(g_router_semaphore, 0, &err);
     if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err);   

    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used && g_service_registry[i].address == address) {
            memcpy(temp_service, &g_service_registry[i], sizeof(service_registry_entry_t));
            if(g_router_mutex)
                infotainment_mutex_post(g_router_mutex);
            return 0;
        }
    }
    //if(g_router_semaphore)
        //infotainment_sem_post(g_router_semaphore);
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex);
    return -1;
}

// Ping-Pong测试相关变量
static ping_test_stats_t g_ping_stats[MAX_SERVICES] = {0};
static bool g_ping_test_running[MAX_SERVICES] = {false};
static uint32_t g_ping_sequence[MAX_SERVICES] = {0};

void send_ping_to_service(service_id_t source_service,service_id_t target_service, const char* ping_msg) {
    ping_pong_payload_t* ping_payload = (ping_pong_payload_t*)infotainment_malloc(sizeof(ping_pong_payload_t));
    if (!ping_payload) {
        BR_ERROR("无法分配ping载荷内存");
        return;
    }
    // 【优化】使用新的查询接口
    service_registry_entry_t source_info = {0};
    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(source_service, &source_info);
    bus_router_get_service_info_by_id(target_service, &target_info);

    if (!source_info.is_used || !target_info.is_used) {
        BR_ERROR("无法找到源或目标服务信息");
        infotainment_free(ping_payload);
        return;
    }

    char index = source_service/SRC_UI;
    if(index < 0 || index >= MAX_SERVICES) return;
    
    memset(ping_payload, 0, sizeof(ping_pong_payload_t));
    ping_payload->ping_id = ++g_ping_sequence[index];
    ping_payload->sequence = g_ping_sequence[index];
    ping_payload->timestamp_send = infotainment_get_ticks();
    ping_payload->source_service = source_service;
    ping_payload->target_service = target_service;
    //eLIBs_snprintf(ping_payload->ping_data, sizeof(ping_payload->ping_data), "%s", ping_msg);

    bus_post_message(source_service, target_info.address, MSG_PRIO_NORMAL, 
                    CMD_PING_REQUEST, ping_payload, SRC_UNDEFINED,NULL);
    
    g_ping_stats[index].total_pings++;
   // BR_PING("🏓 %s发送ping到%s服务 (源:0x%x, 目标:0x%x, 命令:0x%x, ID:%d, 序列:%d)",
    //       source_info.name, target_info.name, source_info.address, target_info.address, CMD_PING_REQUEST,
    //       ping_payload->ping_id, ping_payload->sequence);
}


void service_handle_ping_request(app_message_t* msg) {
    if (!msg || !msg->payload) return;
    ref_counted_payload_t*  wrapper = NULL;
    ping_pong_payload_t* ping_payload = NULL;
    if (msg->payload)
    {
        wrapper = (ref_counted_payload_t*)msg->payload;
        ping_payload = (ping_pong_payload_t*)wrapper->ptr;
    }

    if(!ping_payload) return;

    uint32_t current_time = infotainment_get_ticks();
        // 【优化】使用新的查询接口
    service_registry_entry_t source_info = {0};
    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(msg->source, &source_info);
    bus_router_get_service_info_by_address(msg->destination, &target_info);

    if (!source_info.is_used || !target_info.is_used) {
        BR_ERROR("无法找到源或目标服务信息");
        infotainment_free(ping_payload);
        return;
    }

    BR_PING("🏓 %s收到%s服务的ping请求 (来源:0x%x, 命令:0x%x, ID:%d)",
           target_info.name, source_info.name,source_info.id, msg->command, ping_payload->ping_id);
    
    // 创建pong回复
    ping_pong_payload_t* pong_payload = (ping_pong_payload_t*)infotainment_malloc(sizeof(ping_pong_payload_t));
    if (pong_payload) {
        memcpy(pong_payload, ping_payload, sizeof(ping_pong_payload_t));
        pong_payload->timestamp_recv = current_time;
        pong_payload->rtt_ms = current_time - ping_payload->timestamp_send;
        pong_payload->source_service = target_info.id;
        pong_payload->target_service = source_info.id;
        
        // 修改ping数据表示这是pong回复
       // eLIBs_snprintf(pong_payload->ping_data, sizeof(pong_payload->ping_data), 
         //             "%s pong回复: %s", source_info.name, ping_payload->ping_data);
        
        bus_router_address_t target_address = 0;
        switch (msg->source) {
            case SRC_SYSTEM_SERVICE: target_address = ADDRESS_SYSTEM_SERVICE; break;
            case SRC_RADIO_SERVICE: target_address = ADDRESS_RADIO_SERVICE; break;
            case SRC_UI: target_address = ADDRESS_UI; break;
            case SRC_TIMER_SERVICE: target_address = ADDRESS_TIMER_SERVICE; break;

        }
        
        if (source_info.address) {
            bus_post_message(target_info.id, source_info.address, MSG_PRIO_NORMAL, 
                           CMD_PONG_RESPONSE, pong_payload, SRC_UNDEFINED,NULL);
            BR_PING("✓ %s发送pong回复到%s服务 (目标:0x%x, 命令:0x%x)",
                   target_info.name,source_info.name, target_address, CMD_PONG_RESPONSE);
        } else {
            infotainment_free(pong_payload);
        }
    }
}

void service_handle_pong_response(app_message_t* msg) {
    if (!msg || !msg->payload) return;   
     ref_counted_payload_t*  wrapper = NULL;
    ping_pong_payload_t* pong_payload = NULL;
    
    if(msg->payload)
    {
        wrapper = (ref_counted_payload_t*)msg->payload;
        pong_payload = (ping_pong_payload_t*)wrapper->ptr;
    }    

    service_registry_entry_t source_info = {0};
    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(msg->source, &source_info);
    bus_router_get_service_info_by_address(msg->destination, &target_info);

    if (!source_info.is_used || !target_info.is_used) {
        BR_ERROR("无法找到源或目标服务信息");
        return;
    }

    service_id_t target_service = target_info.id;
    char index = target_service/SRC_UI;
    if(index < 0 || index >= MAX_SERVICES) return;  

    uint32_t current_time = infotainment_get_ticks();
    uint32_t rtt = current_time - pong_payload->timestamp_send;
    
    g_ping_stats[index].total_pongs++;
    
    // 更新RTT统计
    if (g_ping_stats[index].total_pongs == 1) {
        g_ping_stats[index].min_rtt = rtt;
        g_ping_stats[index].max_rtt = rtt;
        g_ping_stats[index].avg_rtt = rtt;
    } else {
        if (rtt < g_ping_stats[index].min_rtt) g_ping_stats[index].min_rtt = rtt;
        if (rtt > g_ping_stats[index].max_rtt) g_ping_stats[index].max_rtt = rtt;
        g_ping_stats[index].avg_rtt = (g_ping_stats[index].avg_rtt * (g_ping_stats[index].total_pongs - 1) + rtt) / g_ping_stats[index].total_pongs;
    }
    

    
    BR_PING("🏓 %s收到%s服务的pong回复 (来源:0x%x, 命令:0x%x):", target_info.name,source_info.name, msg->source, msg->command);
    BR_PING("   - Ping ID: %d", pong_payload->ping_id);
    BR_PING("   - 序列号: %d", pong_payload->sequence);
    BR_PING("   - RTT: %d ms", rtt);
    //BR_PING("   - 数据: %s", pong_payload->ping_data);

    // 内存使用情况 - 使用DEBUG级别
    __u32 free_size = 0, total_size = 0;
	free_size = esMEMS_FreeMemSize();
	total_size = esMEMS_TotalMemSize();
	BR_PING("free memory size is %d", free_size);
	BR_PING("used memory size is %d", total_size-free_size);
	BR_PING("total memory size is %d", total_size);

    
    // 检查是否完成了预期的ping-pong交换
    if (g_ping_stats[index].total_pongs >= g_ping_stats[index].total_test) { // 预期收到3个pong
        show_ping_test_results(target_service);
    }
    else
    {
        char send_str[64] = {0};
        eLIBs_sprintf(send_str, "%s->%s ping test", target_info.name, source_info.name);
        send_ping_to_service(target_service,msg->source, send_str);

    }

}

void show_ping_test_results(service_id_t target_service) {

    char index = target_service/SRC_UI;
    if(index < 0 || index >= MAX_SERVICES) return;  

    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(target_service, &target_info);
    if(!target_info.is_used) return;


    BR_PING("\n🎯 === %s Ping-Pong测试结果 ===",target_info.name);
    BR_PING("总发送ping: %d", g_ping_stats[index].total_pings);
    BR_PING("总接收pong: %d", g_ping_stats[index].total_pongs);
    BR_PING("丢失ping: %d", g_ping_stats[index].total_pings - g_ping_stats[index].total_pongs);
    if (g_ping_stats[index].total_pongs > 0) {
        BR_PING("最小RTT: %d ms", g_ping_stats[index].min_rtt*10);
        BR_PING("最大RTT: %d ms", g_ping_stats[index].max_rtt*10);
        BR_PING("平均RTT: %d ms", g_ping_stats[index].avg_rtt*10);
    }
    BR_PING("=== 测试结果: %s ===\n",
           (g_ping_stats[index].total_pongs == g_ping_stats[index].total_pings) ? "通过" : "失败");

}



//  Ping-Pong测试函数
void start_ping_pong_test(service_id_t source_service,service_id_t target_service, const char* ping_msg,int test_count) {
    
    service_registry_entry_t source_info = {0};
    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(source_service, &source_info);
    bus_router_get_service_info_by_id(target_service, &target_info);

    if (!source_info.is_used || !target_info.is_used) {
        BR_ERROR("无法找到源或目标服务信息");
        return;
    }
    if(test_count <= 0) return;
    char index = source_service/SRC_UI;
    if(index < 0 || index >= MAX_SERVICES) return;

    // 重置统计数据
    memset(&g_ping_stats[index], 0, sizeof(ping_test_stats_t));
    g_ping_stats[index].total_test = test_count;
    g_ping_test_running[index] = true;
    g_ping_sequence[index] = 0;
    
    // 1. 发送ping到System服务
    char send_str[128] = {0};
    BR_PING("\n=== %s开始Ping-Pong通讯测试 ===",source_info.name);
    //eLIBs_sprintf(send_str, "%s->%s ping test", source_info->name, target_info.name);
    send_ping_to_service(source_service,target_service, send_str);

    BR_PING("===Ping-Pong测试启动完成 ===\n");
}

//  broadcast测试函数
void start_broadcast_test(service_id_t source_service) {

    const service_registry_entry_t source_info = {0};
    bus_router_get_service_info_by_id(source_service, &source_info);
    if (!source_info.is_used) {
        BR_ERROR("无法找到源服务信息");
        return;
    }

    bus_router_address_t broadcast_address = ADDRESS_BROADCAST;
    char index = source_service/SRC_UI;
    if(index < 0 || index >= MAX_SERVICES) return;

    // 重置统计数据
    memset(&g_ping_stats[index], 0, sizeof(ping_test_stats_t));
    g_ping_stats[index].total_test = 1;
    g_ping_test_running[index] = true;
    g_ping_sequence[index] = 0;
    
    char send_str[64] = {0};
    broadcast_address = ADDRESS_BROADCAST & (~(source_info.address));

    BR_PING("\n=== %s开始broadcast通讯测试 (排除自己) ===",source_info.name);
    eLIBs_snprintf(send_str, sizeof(send_str), "%s->其他所有服务 ping test", source_info.name);


    ping_pong_payload_t* broadcast_payload = (ping_pong_payload_t*)infotainment_malloc(sizeof(ping_pong_payload_t));
    if (broadcast_payload) {
        memset(broadcast_payload, 0, sizeof(ping_pong_payload_t));
        broadcast_payload->ping_id = ++g_ping_sequence[index];
        broadcast_payload->sequence = g_ping_sequence[index];
        broadcast_payload->timestamp_send = infotainment_get_ticks();
        broadcast_payload->source_service = SRC_UI;
        broadcast_payload->target_service = SRC_UNDEFINED; // 广播
       // eLIBs_sprintf(broadcast_payload->ping_data, "%s广播ping测试", source_info.name);
        
        bus_post_message(source_service, broadcast_address, 
                        MSG_PRIO_NORMAL, CMD_PING_BROADCAST, broadcast_payload,SRC_UNDEFINED, NULL);
        
        g_ping_stats[index].total_pings = g_service_count-1;
        BR_PING("✓ %s发送广播ping (ID:%d)", source_info.name,broadcast_payload->ping_id);
    }

    BR_PING("===broadcast测试启动完成 ===\n");
}




void bus_router_fast_lane_init(void) {
    unsigned char err = 0;
     g_router_fast_lane_mutex = infotainment_mutex_create(0,&err);
    if (!g_router_fast_lane_mutex) {
        BR_ERROR("UI Fast Lane: Failed to create mutex");
        return;
    }
    g_router_fast_lane_semaphore = infotainment_sem_create(0);
    if (!g_router_fast_lane_semaphore) {
        BR_ERROR("UI Fast Lane: Failed to create semaphore");
        infotainment_mutex_delete(g_router_fast_lane_mutex, &err);
        g_router_fast_lane_mutex = NULL;
        return;
    }
    BR_INFO("UI Fast Lane: Initialized");
}

void bus_router_fast_lane_exit(void) {
    unsigned char err = 0;
    if (g_router_fast_lane_mutex) {
        infotainment_mutex_delete(g_router_fast_lane_mutex, &err);
        g_router_fast_lane_mutex = NULL;
    }
    if (g_router_fast_lane_semaphore) {
        infotainment_sem_delete(g_router_fast_lane_semaphore, &err);
        g_router_fast_lane_semaphore = NULL;
    }
    g_head = 0;
    g_tail = 0;
    BR_INFO("UI Fast Lane: Exited");
}

// 【供任何服务调用】发送紧急消息
bool bus_router_fast_lane_post(fast_lane_cmd_t command,
                               fast_lane_priority_t priority,
                               int32_t param1,
                               void* param2) {
    unsigned char err = 0;
    bool success = false;
    if (!g_router_fast_lane_mutex || !g_router_fast_lane_semaphore) {
        BR_ERROR("UI Fast Lane: Not initialized");
        return false; // 未初始化
    }
    
    infotainment_mutex_pend(g_router_fast_lane_mutex, 0, &err);

    uint32_t next_head = (g_head + 1) % FAST_LANE_BUFFER_SIZE;
    if (next_head != g_tail) { // 检查缓冲区是否已满
        g_router_fast_lane_buffer[g_head].command = command;
        g_router_fast_lane_buffer[g_head].priority = priority;
        g_router_fast_lane_buffer[g_head].param1 = param1;
        g_router_fast_lane_buffer[g_head].param2 = param2;
        BR_DEBUG("UI Fast Lane: Posted message with command %d, priority %d", command, priority);
        g_head = next_head;
        success = true;
    } else {
        BR_ERROR("UI Fast Lane: Buffer is full! Dropping message");
    }
    infotainment_mutex_post(g_router_fast_lane_mutex); // 释放互斥锁

    if (success) {
         infotainment_sem_post(g_router_fast_lane_semaphore); // 通知UI线程
    }
    return success;
}

// 【供UI线程调用】一次性读取所有紧急消息
uint32_t bus_router_fast_lane_read_all(fast_lane_msg_t* out_buffer, uint32_t max_count) {
    unsigned char err = 0;    
    uint32_t count = 0;

    if (!out_buffer || max_count == 0 || !g_router_fast_lane_mutex) {
        BR_ERROR("UI Fast Lane: Invalid parameters for reading messages");
        return 0; // 参数无效
    }
    infotainment_mutex_pend(g_router_fast_lane_mutex, 0, &err);

    while (g_tail != g_head && count < max_count) {
        out_buffer[count++] = g_router_fast_lane_buffer[g_tail];
        g_tail = (g_tail + 1) % FAST_LANE_BUFFER_SIZE;
    }

    infotainment_mutex_post(g_router_fast_lane_mutex); // 释放互斥锁
    return count;
}

int bus_router_register_service(
    service_id_t id,
    bus_router_address_t address,
    void* queue_handle,
    const char* name,
    payload_free_func_t free_func,
    payload_copy_func_t copy_func)
{
    unsigned char err = 0; // 错误码

    if(!queue_handle) {
        BR_ERROR("Invalid parameters for service registration");
        return -1; // 参数无效
    }

    if (id <= SRC_UNDEFINED) {
        BR_ERROR("Invalid service ID 0x%x", id);
        return -1; // 无效的服务ID
    }

    BR_DEBUG("Registering service %s-0x%x with address 0x%llx and queue handle %p",name, id, address, queue_handle);
    
   // if(g_router_semaphore)
        //infotainment_sem_wait(g_router_semaphore, 0, &err); // 等待信号量，确保线程安全   

     if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err);   
   
    // 1. 检查服务是否已存在
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used && g_service_registry[i].id == id) {
            BR_INFO("Service 0x%x already registered", id);
            //if(g_router_semaphore)
                //infotainment_sem_post(g_router_semaphore);
            if(g_router_mutex)
                infotainment_mutex_post(g_router_mutex);
            return 0; // 已存在，返回成功
        }
    }

    // 2. 检查是否还有空槽位
    if (g_service_count >= MAX_SERVICES) {
        BR_ERROR("Failed to register service 0x%x, registry full", id);
        if(g_router_mutex)
            infotainment_mutex_post(g_router_mutex);
        //if(g_router_semaphore)
            //infotainment_sem_post(g_router_semaphore);
        return -1; // 注册表已满
    }

    // 3. 找到第一个空槽位并注册服务
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (!g_service_registry[i].is_used) {
            g_service_registry[i].id = id;
            g_service_registry[i].address = address;
            g_service_registry[i].queue_handle = queue_handle;
            g_service_registry[i].name = name;
            g_service_registry[i].free_func = free_func;
            g_service_registry[i].copy_func = copy_func;
            g_service_registry[i].is_used = true;
            g_service_count++;
            BR_INFO("Service 0x%x registered successfully at slot %d", id, i);
           // if(g_router_semaphore)
              //  infotainment_sem_post(g_router_semaphore);
            if(g_router_mutex)
                infotainment_mutex_post(g_router_mutex);
            return 0; // 成功
        }
    }

    // 4. 理论上不应该到达这里
    BR_ERROR("Unexpected error during service registration");
    //if(g_router_semaphore)
       // infotainment_sem_post(g_router_semaphore);
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex);
    return -1;
}

int bus_router_unregister_service(service_id_t id) {
    unsigned char err = 0; // 错误码

    if (id <= SRC_UNDEFINED) {
        BR_ERROR("Invalid service ID 0x%x", id);
        return -1; // 无效的服务ID
    }
    
    if (g_router_exiting) {
        BR_ERROR("Router is exiting, cannot unregister service 0x%x", id);
        return -1; // 路由器正在退出，不能注销服务
    }

    //if(g_router_semaphore)
        //infotainment_sem_wait(g_router_semaphore, 0, &err); // 等待信号量，确保线程安全   
    if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err);

    // 1. 查找并删除服务条目
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used && g_service_registry[i].id == id) {
            BR_INFO("Unregistering service 0x%x from slot %d", id, i);
            
            // 清空该槽位
            memset(&g_service_registry[i], 0, sizeof(service_registry_entry_t));
            g_service_registry[i].is_used = false;
            g_service_count--;
            
            BR_INFO("Service 0x%x unregistered successfully", id);
            //if(g_router_semaphore)
                //infotainment_sem_post(g_router_semaphore);
            if(g_router_mutex)
                infotainment_mutex_post(g_router_mutex);
            return 0; // 成功
        }
    }   
    
    BR_ERROR("Service 0x%x not found in registry", id);
    //if(g_router_semaphore)
        //infotainment_sem_post(g_router_semaphore);
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex);
    return -1; // 未找到服务条目
}

/**
 * @brief 根据 service_id 查找对应的 payload 拷贝函数
 */
void* (*find_payload_copy_func(service_id_t source_id))(const void*) {
    service_registry_entry_t info = {0};
    bus_router_get_service_info_by_id(source_id, &info);
    if (info.is_used) {
        return info.copy_func;
    }
    return NULL;
}



// 【重要】提供一个函数来释放带引用计数的 payload
void free_ref_counted_payload(ref_counted_payload_t* wrapper) {
    unsigned char err = 0;
    if (!wrapper) return;
    
    // 通常需要加锁保护 ref_count 的并发访问
    //infotainment_sem_wait(g_router_semaphore, 0, &err);
    if(g_router_mutex)
        infotainment_mutex_pend(g_router_mutex, 0, &err); 
    // 【核心修改】使用新的、专门的 DUMP 宏
    //BR_DUMP_WRAPPER(wrapper, "Attempting to free (before decref)");
        // [添加日志]
    BR_DEBUG("[PAYLOAD_TRACE] Decref wrapper %p, ref_count from %d to %d. Payload ptr: %p\n",
           wrapper, wrapper->ref_count, wrapper->ref_count - 1, wrapper->ptr);

    if(wrapper->ref_count > 0)
        wrapper->ref_count--;

    if (wrapper->ref_count <= 0) {    
        // [添加日志]
        BR_DEBUG("[PAYLOAD_TRACE] RefCount is ZERO for wrapper %p. Preparing to free payload %p.\n",
               wrapper, wrapper->ptr); 
        // 当引用计数为0时，真正释放内存
        if (wrapper->free_func && wrapper->ptr) {
            // [添加日志]
            BR_DEBUG("[PAYLOAD_TRACE] -> Using CUSTOM free_func %p for payload %p\n",
                   wrapper->free_func, wrapper->ptr);
            wrapper->free_func(wrapper->ptr); // 调用专用的释放函数
        }
        else
        {
            // [添加日志]
            BR_DEBUG("[PAYLOAD_TRACE] -> Using GENERIC infotainment_free for payload %p\n",
                   wrapper->ptr);
            BR_DEBUG("Bus Router: No specific free function found for payload. Using generic free.\n");
            if(wrapper->ptr)
                infotainment_free(wrapper->ptr); // 使用通用的释放函数
        }
        // [添加日志]
        BR_DEBUG("[PAYLOAD_TRACE] Now freeing the wrapper %p itself.\n", wrapper);
        wrapper->ptr = NULL; // 清空指针，避免悬挂指针
        infotainment_free(wrapper); // 释放包装器本身
    }
    //infotainment_sem_post(g_router_semaphore); // 等待信号量，确保线程安全  
    if(g_router_mutex)
        infotainment_mutex_post(g_router_mutex); 
}

int bus_post_message(service_id_t source, bus_router_address_t destination_mask,message_priority_t  priority, unsigned int command, void* payload,service_id_t payload_source, void* reserved) {

    ref_counted_payload_t* payload_wrapper = NULL;
    int destinations_found = 0;

    BR_DEBUG("Posting message: source=0x%x, dest_mask=0x%llx, priority=%d, cmd=0x%x", 
             source, destination_mask, priority, command);

    if(g_router_exiting)
    {
        BR_ERROR("Router is exiting, cannot post message");
        goto error; // 路由器正在退出，不能发送消息
    }

    if (source == SRC_UNDEFINED || destination_mask == ADDRESS_NONE) {
        BR_ERROR("Invalid source (0x%x) or destination (0x%llx)", source, destination_mask);
         goto error; // 无效的服务ID
    }
    if (priority < MSG_PRIO_LOW || priority >= MSG_PRIO_COUNT) {
        BR_ERROR("Invalid message priority %d", priority);
         goto error; // 无效的优先级
    }

    
    // 1. 分配消息结构体
    app_message_t* msg = (app_message_t*)infotainment_malloc(sizeof(app_message_t));
    if (!msg) {
        BR_ERROR("Failed to allocate memory for message");
        goto error; // 内存分配失败
    }   
    // 2. 初始化消息字段
    msg->source = source; // 设置消息来源
    msg->destination = destination_mask; // 设置消息目的地
    msg->priority = priority; // 设置消息优先级
    msg->command = command; // 设置消息命令
    msg->payload = payload; // 设置消息负载
    msg->payload_source = payload_source; // 设置消息负载的真正来源
    msg->reserved = reserved; // 设置保留字段
    // 3. 将消息投递到路由器的主入口队列
    if (!g_router_queue_handle[priority]) {
        BR_ERROR("Router queue handle is NULL, cannot post message");
        goto error; // 路由器队列未初始化
    }
    // 4. 打印调试信息
    if (msg->payload) {
        BR_DEBUG("Posting message from 0x%x to 0x%llx (cmd: 0x%x, payload addr: %p)", 
                 msg->source, msg->destination, msg->command, msg->payload);    
    } else {
        BR_DEBUG("Posting message from 0x%x to 0x%llx (cmd: 0x%x, no payload)", 
                 msg->source, msg->destination, msg->command);
    }
    // 5. 将消息投递到路由器的主入口队列
    // 注意：这里假设infotainment_queue_post函数可以处理void*类型的消息
    if (infotainment_queue_post(g_router_queue_handle[priority], (void*)msg) != OS_NO_ERR) {
        BR_ERROR("Failed to post message to router queue");
        goto error; // 投递失败
    }
    BR_DEBUG("Message posted successfully to router queue");

    return 0; // 成功发送消息
    error:
     BR_ERROR("Failed to post message");
    if (payload) {
        const service_registry_entry_t source_info = {0};
        bus_router_get_service_info_by_id(msg->payload_source, &source_info);
        if (source_info.is_used && source_info.free_func) 
        {
            source_info.free_func(payload);
        }
        else
        {
            BR_DEBUG("Bus Router: No specific free function found for payload. Using generic free.\n");
            infotainment_free(payload);
        }
        payload = NULL;
    }
    if (msg) {
        infotainment_free(msg);
        msg = NULL;
    }
    return -1; // 发送消息失败
}




// 路由器线程主函数
static void router_thread_entry(void* p_arg) {
    app_message_t* msg = NULL; // 接收的消息指针  
    ref_counted_payload_t* payload_wrapper = NULL; // 引用计数的 payload 包装器
    void* dest_queue_handle = NULL; // 目标服务的队列句柄
    int i = 0;
    unsigned char err = 0;        
    int destinations_found = 0;
    bool any_message_processed = EPDK_FALSE; // 标志是否处理了任何消息
    BR_INFO("Router thread started.");

    while (!g_router_exiting) {
        if (infotainment_thread_TDelReq(EXEC_prioself))
        {
            BR_INFO("Router thread: Received delete request, exiting...");
            goto exit;
        }
        //if(g_router_semaphore)
            ///infotainment_sem_wait(g_router_semaphore, 0,&err); // 等待信号量，确保线程安全  
        //if(g_router_mutex)
            //infotainment_mutex_pend(g_router_mutex, 0, &err);


        any_message_processed = EPDK_FALSE;
        // . 遍历服务注册表，查找所有匹配的目的地

        for (int prio = (MSG_PRIO_COUNT-1); prio >= MSG_PRIO_LOW; --prio) {        // 1. 阻塞等待，从自己的主入口队列接收任何消息 
            if (!g_router_queue_handle[prio]) {
                BR_ERROR("Router: No queue handle for priority %d, skipping...", prio);
                continue; // 如果没有对应优先级的队列，跳过
            }
            while(1)
            {
                msg = infotainment_queue_get(g_router_queue_handle[prio], &err);
                if (msg && err == OS_NO_ERR) {
                    any_message_processed = EPDK_TRUE;
                    BR_DEBUG("Received msg (cmd: 0x%x) from service 0x%x to dest 0x%llx", 
                             msg->command, msg->source, msg->destination);
                    
                    // 2. 根据消息目的地查找目标队列句柄
                    dest_queue_handle = NULL;
                    // 遍历服务注册表，查找目标服务的队列句柄
                    BR_DEBUG("Looking for destination services matching mask 0x%llx", msg->destination);
                    if (msg->destination == SRC_UNDEFINED) {
                        BR_ERROR("Message has undefined destination, dropping message");
                        if (msg->payload)
                        {
                            service_registry_entry_t source_info = {0};
                            bus_router_get_service_info_by_id(msg->payload_source, &source_info);
                            if (source_info.is_used && source_info.free_func) 
                            {
                                source_info.free_func(msg->payload);
                            }
                            else
                            {
                                BR_DEBUG("Bus Router: No specific free function found for payload. Using generic free.\n");
                                if(msg->payload)
                                    infotainment_free(msg->payload);
                            }

                        }
                        infotainment_free(msg);
                        break; // 继续等待下一个消息
                    }

                    // 2. 准备引用计数的 payload
                    if (msg->payload) {
                        // 为原始 payload 创建引用计数包装器
                        payload_wrapper = (ref_counted_payload_t*)infotainment_malloc(sizeof(ref_counted_payload_t));
                         // 【优化】通过查询注册表来获取 free 函数
                        service_registry_entry_t source_info = {0};
                        bus_router_get_service_info_by_id(msg->payload_source,&source_info);
                       
                        if (payload_wrapper) {
                             if (source_info.is_used) {
                                payload_wrapper->free_func = source_info.free_func;
                            } else {
                                payload_wrapper->free_func = NULL;
                                BR_INFO("No free function found for payload source 0x%x", msg->payload_source);
                            }
                            payload_wrapper->ptr = msg->payload;
                            payload_wrapper->ref_count = 0; // 稍后根据找到的目的地数量来设置
                        } else {
                            BR_ERROR("Bus Router: Failed to allocate payload wrapper. Payload will be dropped.\n");
                            // 无法创建包装器，必须释放原始 payload
                            if (source_info.is_used && source_info.free_func) 
                            {
                                source_info.free_func(msg->payload);
                            }
                            else
                            {
                                BR_DEBUG("Bus Router: No specific free function found for payload. Using generic free.\n");
                                infotainment_free(msg->payload);
                            }
                            // 释放消息体，然后继续下一次循环
                            infotainment_free(msg);
                            break;
                        }
                    }
                    else
                    {
                        payload_wrapper = NULL;
                    }


                    if(g_router_mutex)
                        infotainment_mutex_pend(g_router_mutex, 0, &err);

                    destinations_found = 0;
                    // 遍历服务注册表数组，查找匹配的目标服务
                    for (int i = 0; i < MAX_SERVICES; i++) {
                        if (!g_service_registry[i].is_used) {
                            continue; // 跳过未使用的槽位
                        }
                        
                        if (g_service_registry[i].address & msg->destination) {
                            dest_queue_handle = g_service_registry[i].queue_handle;
                            destinations_found++;
                            if (payload_wrapper)
                                payload_wrapper->ref_count++;
                            BR_DEBUG("Found matching destination service ID 0x%x at slot %d", g_service_registry[i].id, i);
                            
                            // 4. 为这个目的地创建一个新的消息副本
                            app_message_t* msg_for_dest = (app_message_t*)infotainment_malloc(sizeof(app_message_t));
                            if (!msg_for_dest) {
                                BR_ERROR("Failed to allocate message for destination 0x%x", g_service_registry[i].id);
                                // 如果无法为某个目标分配消息，我们只是跳过它，但要确保引用计数最终是正确的
                                destinations_found--; 
                                if (payload_wrapper)
                                    payload_wrapper->ref_count--;
                                continue;
                            }
                            
                            // 拷贝原始消息的所有字段
                            memcpy(msg_for_dest, msg, sizeof(app_message_t));
                            
                            // 【重要】将消息副本的 payload 指针替换为指向共享的包装器
                            // 我们约定 app_message_t 的 payload 字段可以被这样复用
                            msg_for_dest->payload = (void*)payload_wrapper;
                            msg_for_dest->destination = g_service_registry[i].address;

                            // 5. 将消息副本投递到目标服务的队列中
                           // BR_DEBUG("Posting message to service 0x%x, queue handle %p", g_service_registry[i].id, g_service_registry[i].queue_handle);
                            BR_DUMP_MESSAGE(msg_for_dest, "Posting to Destination");
                            if (infotainment_queue_post(g_service_registry[i].queue_handle, msg_for_dest) != OS_NO_ERR) {
                                BR_ERROR("Failed to post to queue for service %s-0x%x", g_service_registry[i].name,g_service_registry[i].id);
                                // 投递失败，释放刚刚创建的消息副本
                                infotainment_free(msg_for_dest);
                                destinations_found--; // 同样需要递减计数
                                if (payload_wrapper)
                                    payload_wrapper->ref_count--;
                            } else {
                                BR_DEBUG("Successfully posted message to service 0x%x", g_service_registry[i].id);
                            }

                        }
                    }
                    
                     // 根据最终成功投递的目的地数量，设置引用计数
                    if (payload_wrapper) {
                        //infotainment_mutex_pend(g_router_mutex, 0, &err);
                        //payload_wrapper->ref_count = destinations_found;
                        //infotainment_mutex_post(g_router_mutex);

                        // 如果最终没有一个成功的目的地，但我们创建了包装器，
                        // 那么路由器线程必须负责释放它。
                        if (destinations_found == 0) {
                            BR_ERROR("Bus Router: Message to 0x%llx had no valid destinations. Freeing payload.\n", msg->destination);
                            // 此时 ref_count 为 0，调用此函数会直接释放所有相关内存
                            if (payload_wrapper->free_func && payload_wrapper->ptr) {
                                payload_wrapper->free_func(payload_wrapper->ptr); // 调用专用的释放函数
                            }
                            else
                            {
                                BR_DEBUG("Bus Router: No specific free function found for payload. Using generic free.\n");
                                if(payload_wrapper->ptr)
                                    infotainment_free(payload_wrapper->ptr); // 使用通用的释放函数
                            }
                            payload_wrapper->ptr = NULL; // 清空指针，避免悬挂指针
                            infotainment_free(payload_wrapper); // 释放包装器本身
                        }
                    }
                        
                    if(g_router_mutex)
                        infotainment_mutex_post(g_router_mutex);

                    // . 释放从中央队列中取出的原始消息体
                    // 注意：它的 payload 已经被包装起来，所有权已经转移，所以不能在这里释放 msg->payload
                    infotainment_free(msg);
                    msg = NULL;

                    
                }
                else{
                    if (err != EPDK_OK) {
                        //BR_INFO("Router: Failed to get message from queue for priority %d, error code: %d\n", prio, err);
                    }
                    break; // 队列为空或出错，跳出循环继续处理下一个优先级  
                }


            }

        }     
        //if(g_router_semaphore)
            //infotainment_sem_post(g_router_semaphore); // 等待信号量，确保线程安全
        //if(g_router_mutex)
            //infotainment_mutex_post(g_router_mutex);

        if(any_message_processed == EPDK_FALSE)
        {
            // 5. 如果没有消息被处理，延时1ms
            //BR_DEBUG("No message processed, sleeping for 10ms...");
            infotainment_thread_sleep(1);
        }
       
    }

    exit:
    // 线程退出前的清理工作
    BR_INFO("Router thread exiting...");
    infotainment_thread_delete(g_router_thread_handle);

}

// --- 公共API实现 ---

void bus_router_init(void) {
    int i = 0;
    unsigned char err = 0;
    BR_INFO("Initializing Bus Router...");
    
    // 1. 初始化服务注册表数组
    memset(g_service_registry, 0, sizeof(g_service_registry));
    g_service_count = 0;
    for (i = 0; i < MAX_SERVICES; i++) {
        g_service_registry[i].is_used = false;
    }

    bus_router_fast_lane_init(); // 初始化紧急指令通道
    //g_router_semaphore = infotainment_sem_create(1); // 创建信号量用于线程同步
    //if (!g_router_semaphore) {
    //    BR_ERROR("Failed to create router semaphore");
    //    goto error; // 信号量创建失败
    //}

     g_router_mutex = infotainment_mutex_create(0,&err);
    if (!g_router_mutex) {
        BR_ERROR("Bus Router: Failed to create mutex");
        return;
    }

    // 2. 创建路由器自己的主入口队列
    for(i = 0; i < MSG_PRIO_COUNT; i++) {
        g_router_queue_handle[i] = infotainment_queue_create(ROUTER_QUEUE_SIZE);
        if (!g_router_queue_handle[i]) {
            BR_ERROR("Failed to create router queue for priority %d", i);
            goto error;
        }
        BR_DEBUG("Created router queue handle %p for priority %d", g_router_queue_handle[i], i);
    }

    // 3. 创建并启动路由器线程
     g_router_thread_handle = infotainment_thread_create(router_thread_entry, NULL, TASK_PROI_LEVEL5, 1024*8, "BusRouterThread");
    if (g_router_thread_handle < 0) {
        BR_ERROR("Failed to create router thread");
        goto error;
    }
    BR_INFO("Bus Router initialized and thread started");    
    g_router_exiting = 0;
    return; // 初始化成功

    error:
    for(i = 0; i < MSG_PRIO_COUNT; i++) {
        if (g_router_queue_handle[i]) {
            infotainment_queue_flush(g_router_queue_handle[i]);
            infotainment_queue_delete(g_router_queue_handle[i],&err);
            g_router_queue_handle[i] = NULL; // 清理已创建的队列
        }
    }
    if(g_router_thread_handle>0)    
    {
        while(infotainment_thread_delete_req(g_router_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            infotainment_thread_sleep(10);
        }
        g_router_thread_handle = 0; // 清理线程句柄
    }
    if(g_router_mutex) {
        infotainment_mutex_delete(g_router_mutex, &err);
        g_router_mutex = NULL; // 清理互斥锁
    }

    //if(g_router_semaphore) {
     //   infotainment_sem_delete(g_router_semaphore, &err);
   //     g_router_semaphore = NULL; // 清理信号量
   // }
    bus_router_fast_lane_exit(); // 清理紧急指令通道
    return; // 初始化失败，退出
}

void bus_router_exit(void) {
    int i = 0;
    unsigned char err = 0;
    if (g_router_thread_handle < 0) {
        BR_INFO("Router thread not running, nothing to exit");
        return; // 路由器线程未运行，无需退出
    }
    BR_INFO("Exiting Bus Router...");
    g_router_exiting = 1; // 设置退出标志，通知线程退出

     // 1. 删除路由器线程
    if(g_router_thread_handle>0)    
    {
        //if(g_router_semaphore)
            //infotainment_sem_wait(g_router_semaphore, 0, &err);    
        if(g_router_mutex)
            infotainment_mutex_pend(g_router_mutex, 0, &err);
        while(infotainment_thread_delete_req(g_router_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            infotainment_thread_sleep(10);
        }
        g_router_thread_handle = 0; // 清理线程句柄
        //if(g_router_semaphore)
            //infotainment_sem_post(g_router_semaphore);
        if(g_router_mutex)
            infotainment_mutex_post(g_router_mutex);

    }

     // 2. 删除路由器主入口队列
    for (i = 0; i < MSG_PRIO_COUNT; i++) {
        if (g_router_queue_handle[i]) {
            void* msg = NULL;
            unsigned char err = 0;
            // 不断从队列取出消息并释放，直到队列为空
            while ((msg = infotainment_queue_get(g_router_queue_handle[i], &err)) && err == OS_NO_ERR) {
                app_message_t* app_msg = (app_message_t*)msg;
                if (app_msg && app_msg->payload) {
                    free_ref_counted_payload((ref_counted_payload_t *)app_msg->payload);
                }
                if(app_msg)
                    infotainment_free(app_msg);
            }
            infotainment_queue_delete(g_router_queue_handle[i], &err);
            g_router_queue_handle[i] = NULL;
        }
    }

    // 3. 清理服务注册表数组
    for (i = 0; i < MAX_SERVICES; i++) {
        if (g_service_registry[i].is_used) {
            if (g_service_registry[i].queue_handle) {
                // 注意：这里不删除服务的队列，因为那是服务自己的责任
                // infotainment_queue_delete(g_service_registry[i].queue_handle, &err);
            }
            memset(&g_service_registry[i], 0, sizeof(service_registry_entry_t));
            g_service_registry[i].is_used = false;
        }
    }
    g_service_count = 0;

    if(g_router_mutex) {
        infotainment_mutex_delete(g_router_mutex, &err);
        g_router_mutex = NULL; // 清理互斥锁
    }

    //if(g_router_semaphore) {
    //    infotainment_sem_delete(g_router_semaphore, &err);
     //   g_router_semaphore = NULL; // 清理信号量
    //}   
    bus_router_fast_lane_exit(); // 清理紧急指令通道

    BR_INFO("Bus Router exited successfully");
}

// ================================
// 消息打印接口实现
// ================================

/**
 * @brief 打印 app_message_t 消息的基本信息
 * @param msg 要打印的消息指针
 * @param prefix 打印前缀字符串
 */
void bus_router_print_message(const app_message_t* msg, const char* prefix) {
    if (!msg) {
        printf("%s[MSG-PRINT] NULL message pointer\n", prefix ? prefix : "");
        return;
    }

    const char* pre = prefix ? prefix : "[MSG-PRINT]";

    // 获取源服务信息
    service_registry_entry_t source_info = {0};
    bus_router_get_service_info_by_id(msg->source, &source_info);
    const char* source_name =  source_info.is_used ? source_info.name : "Unknown";

    // 获取目标服务信息（如果是单一目标）
    service_registry_entry_t dest_info = {0};
    bus_router_get_service_info_by_address(msg->destination, &dest_info);
    const char* dest_name = dest_info.is_used ? dest_info.name : "Multiple/Unknown";

    printf("%s ========== Message Info ==========\n", pre);
    printf("%s Source:      0x%08X (%s)\n", pre, msg->source, source_name);
    printf("%s Destination: 0x%016llX (%s)\n", pre, msg->destination, dest_name);
    printf("%s Priority:    %d\n", pre, msg->priority);
    printf("%s Command:     0x%08X\n", pre, msg->command);
    printf("%s Payload:     %p\n", pre, msg->payload);
    printf("%s PayloadSrc:  0x%08X\n", pre, msg->payload_source);
    printf("%s Reserved:    %p\n", pre, msg->reserved);
    printf("%s ==================================\n", pre);
}

/**
 * @brief 打印 app_message_t 消息的详细信息，包括payload内容
 * @param msg 要打印的消息指针
 * @param prefix 打印前缀字符串
 * @param show_payload 是否显示payload内容 (1=显示, 0=不显示)
 */
void bus_router_print_message_detailed(const app_message_t* msg, const char* prefix, int show_payload) {
    if (!msg) {
        printf("%s[MSG-PRINT] NULL message pointer\n", prefix ? prefix : "");
        return;
    }

    const char* pre = prefix ? prefix : "[MSG-PRINT]";

    // 先打印基本信息
    bus_router_print_message(msg, prefix);

    if (!show_payload || !msg->payload) {
        return;
    }

    printf("%s ========== Payload Details =======\n", pre);

    // 尝试解析payload内容
    ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;

    // 检查是否是引用计数包装器
    if (wrapper && wrapper->ptr) {
        printf("%s Payload Type: ref_counted_payload_t\n", pre);
        printf("%s Ref Count:   %d\n", pre, wrapper->ref_count);
        printf("%s Free Func:   %p\n", pre, wrapper->free_func);
        printf("%s Data Ptr:    %p\n", pre, wrapper->ptr);

        // 根据命令类型尝试解析具体的payload内容
        if (msg->command == CMD_PING_REQUEST || msg->command == CMD_PONG_RESPONSE || msg->command == CMD_PING_BROADCAST) {
            ping_pong_payload_t* ping_payload = (ping_pong_payload_t*)wrapper->ptr;
            if (ping_payload) {
                printf("%s --- Ping-Pong Payload ---\n", pre);
                printf("%s Ping ID:      %u\n", pre, ping_payload->ping_id);
                printf("%s Sequence:     %u\n", pre, ping_payload->sequence);
                printf("%s Send Time:    %u\n", pre, ping_payload->timestamp_send);
                printf("%s Recv Time:    %u\n", pre, ping_payload->timestamp_recv);
                printf("%s RTT:          %u ms\n", pre, ping_payload->rtt_ms);
                printf("%s Source Svc:   0x%08X\n", pre, ping_payload->source_service);
                printf("%s Target Svc:   0x%08X\n", pre, ping_payload->target_service);
               // printf("%s Data:         \"%.63s\"\n", pre, ping_payload->ping_data);
            }
        } else {
            // 对于其他类型的payload，显示前32字节的十六进制内容
            printf("%s --- Raw Payload Data (first 32 bytes) ---\n", pre);
            uint8_t* data = (uint8_t*)wrapper->ptr;
            for (int i = 0; i < 32 && i < 256; i += 16) { // 假设最大256字节
                printf("%s %04X: ", pre, i);
                for (int j = 0; j < 16 && (i + j) < 32; j++) {
                    printf("%02X ", data[i + j]);
                }
                printf("\n");
            }
        }
    } else {
        // 直接的payload指针
        printf("%s Payload Type: Direct pointer\n", pre);
        printf("%s Data Ptr:    %p\n", pre, msg->payload);

        // 显示前32字节的十六进制内容
        printf("%s --- Raw Data (first 32 bytes) ---\n", pre);
        uint8_t* data = (uint8_t*)msg->payload;
        for (int i = 0; i < 32; i += 16) {
            printf("%s %04X: ", pre, i);
            for (int j = 0; j < 16 && (i + j) < 32; j++) {
                printf("%02X ", data[i + j]);
            }
            printf("\n");
        }
    }

    printf("%s ==================================\n", pre);
}


