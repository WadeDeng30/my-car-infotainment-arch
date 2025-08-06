#ifndef BUS_ROUTER_H
#define BUS_ROUTER_H
#include <message_defs.h>
#include <infotainment_os_api.h>
#include <service_system.h>
#include <service_radio.h>
#include <service_timer.h>
#include <service_msg.h>
#include <service_mcu.h>

#define ENABLE_BUS_TESTING


// Ping-Pong测试命令定义
#define CMD_PING_REQUEST        0x1001  // Ping请求
#define CMD_PONG_RESPONSE       0x1002  // Pong响应
#define CMD_PING_BROADCAST      0x1003  // Ping广播
#define CMD_PING_TEST_START     0x1004  // 开始Ping测试
#define CMD_PING_TEST_STOP      0x1005  // 停止Ping测试
#define CMD_PING_TEST_RESULT    0x1006  // Ping测试结果

// Ping-Pong载荷结构体
typedef struct {
    uint32_t ping_id;           // Ping ID
    uint32_t sequence;          // 序列号
    uint32_t timestamp_send;    // 发送时间戳
    uint32_t timestamp_recv;    // 接收时间戳
    uint32_t rtt_ms;           // 往返时间(毫秒)
    service_id_t source_service; // 源服务ID
    service_id_t target_service; // 目标服务ID
    //char ping_data[128];        // Ping数据
} ping_pong_payload_t;

// Ping测试统计结构体
typedef struct {
    uint32_t total_pings;      // 总发送数
    uint32_t total_pongs;      // 总接收数
    uint32_t lost_pings;       // 丢失数
    uint32_t total_test;      // 测试总数
    uint32_t min_rtt;          // 最小RTT
    uint32_t max_rtt;          // 最大RTT
    uint32_t avg_rtt;          // 平均RTT
    service_id_t test_partner; // 测试伙伴服务ID
} ping_test_stats_t;

// 测试载荷结构体
typedef struct {
    int test_id;                    // 测试ID
    char test_data[128];           // 测试数据
    uint32_t timestamp;            // 时间戳
} test_payload_t;




#define MAX_SERVICES 64
#define ROUTER_QUEUE_SIZE 256
#define FAST_LANE_BUFFER_SIZE 16 // 缓冲区大小，例如16条

extern void *g_router_fast_lane_semaphore;
// 【新增】定义 payload 管理函数的指针类型
typedef void (*payload_free_func_t)(void*);
typedef void* (*payload_copy_func_t)(const void*);

// 服务注册表条目
typedef struct service_registry_entry {
    service_id_t id;
    bus_router_address_t address;
    void* queue_handle; // 假设队列句柄是void*类型
    const char*          name; // 服务名称:
    bool is_used; // 标记该槽位是否被使用
    payload_free_func_t  free_func; // 专用的 payload 释放函数
    payload_copy_func_t  copy_func; // 专用的 payload 拷贝函数
} service_registry_entry_t;


void bus_router_init(void) ;
void bus_router_exit(void) ;
// 【核心修改】注册函数，增加函数指针参数
int bus_router_register_service(
    service_id_t id,
    bus_router_address_t address,
    void* queue_handle,
    const char* name,
    payload_free_func_t free_func, 
    payload_copy_func_t copy_func
);
int bus_router_unregister_service(service_id_t id);
int bus_post_message(
    service_id_t source,
    bus_router_address_t destination_mask,
    message_priority_t priority,
    unsigned int command,
    void* payload,
    service_id_t payload_source, // 【新增】payload的真正来源ID
    void* reserved
);
void free_ref_counted_payload(ref_counted_payload_t* wrapper);
bool bus_router_fast_lane_post(fast_lane_cmd_t command,
                               fast_lane_priority_t priority,
                               int32_t param1,
                               void* param2);
uint32_t bus_router_fast_lane_read_all(fast_lane_msg_t* out_buffer, uint32_t max_count);

// Debug functions
void bus_router_set_debug(int enable);
void bus_router_set_info(int enable);
void bus_router_set_ping_pong_log(int enable);

void send_ping_to_service(service_id_t source_service,service_id_t target_service, const char* ping_msg);
void service_handle_ping_request(app_message_t* msg);
void service_handle_pong_response(app_message_t* msg);
void show_ping_test_results(service_id_t target_service);
void start_ping_pong_test(service_id_t source_service,service_id_t target_service, const char* ping_msg,int test_count);
void start_broadcast_test(service_id_t source_service);
int bus_router_get_service_info_by_id(service_id_t id,service_registry_entry_t *temp_service);
int bus_router_get_service_info_by_address(bus_router_address_t address,service_registry_entry_t *temp_service);
void* (*find_payload_copy_func(service_id_t source_id))(const void*);

// 【新增】消息打印接口
void bus_router_print_message(const app_message_t* msg, const char* prefix);
void bus_router_print_message_detailed(const app_message_t* msg, const char* prefix, int show_payload);

#endif