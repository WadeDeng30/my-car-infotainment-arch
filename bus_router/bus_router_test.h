// --- START OF NEW FILE bus_router_test.h ---

#ifndef BUS_ROUTER_TEST_H
#define BUS_ROUTER_TEST_H

#include "message_defs.h" // 需要使用 service_id_t 等定义

/**
 * @brief 测试模式的枚举
 */
typedef enum {
    BUS_TEST_MODE_MEMORY_LEAK,      // 内存泄漏测试
    BUS_TEST_MODE_PRIORITY,         // 优先级测试
    BUS_TEST_MODE_STRESS_SINGLE,    // 单目标抗压测试
    BUS_TEST_MODE_STRESS_BROADCAST, // 广播抗压测试
    BUS_TEST_MODE_LATENCY           // 消息延迟测试 (Ping-Pong)
} bus_test_mode_t;

/**
 * @brief 启动总线测试
 * @param test_mode      要执行的测试模式
 * @param source_service 发起测试的服务ID
 * @param target_service 接收测试的服务ID (或广播中的一个主要目标)
 * @param num_messages   要发送的消息数量
 * @param payload_size   每个消息的 payload 大小 (0表示无payload)
 */
void bus_router_start_test(
    bus_test_mode_t test_mode,
    service_id_t source_service,
    service_id_t target_service,
    int num_messages,
    int payload_size
);

/**
 * @brief 打印当前内存使用快照 (用于对比)
 * @param stage_name 阶段名称 (e.g., "Before Test")
 */
void bus_router_print_memory_snapshot(const char* stage_name);

#endif // BUS_ROUTER_TEST_H