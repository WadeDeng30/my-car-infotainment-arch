// --- START OF NEW FILE bus_router_test.c ---

#include "bus_router_test.h"
#include "bus_router.h"     // 需要调用 bus_router 的核心API
#include "infotainment_os_api.h"
#include <string.h>
#include <stdio.h>

// --- 条件编译宏 ---
// 在您的 Kconfig 或 Makefile 中定义 ENABLE_BUS_TESTING 来启用这整个文件
#ifdef ENABLE_BUS_TESTING

// --- 测试专用定义 ---
#define TEST_CMD_MEMORY_LEAK      0xDEAD0001
#define TEST_CMD_PRIORITY_LOW     0xDEAD0002
#define TEST_CMD_PRIORITY_NORMAL  0xDEAD0003
#define TEST_CMD_PRIORITY_HIGH    0xDEAD0004
#define TEST_CMD_STRESS           0xDEAD0005

#define TEST_SERVICE_ID         ((service_id_t)0xFF0000)
#define TEST_SERVICE_ADDRESS    ((bus_router_address_t)1 << 30)
#define TEST_SERVICE_NAME       "TestReceiverService"

// --- 测试服务上下文 ---
typedef struct {
    int expected_messages;
    volatile int received_messages;
    // 用于优先级测试
    volatile int received_low_prio;
    volatile int received_normal_prio;
    volatile int received_high_prio;
    message_priority_t last_received_prio;
    bool priority_test_failed;
    // 用于内存快照
    uint32_t mem_before;
    uint32_t mem_after;
} test_service_context_t;

static test_service_context_t g_test_ctx;
static void* g_test_service_queue = NULL;
static int g_test_service_thread = 0;
static volatile bool g_test_thread_running = false;

// --- 测试服务的线程 ---
static void test_service_thread_entry(void* p_arg) {
    unsigned char err;
    printf("[BusTest] Test Receiver Service thread started.\n");
    g_test_thread_running = true;

    while (g_test_thread_running) {
        app_message_t* msg = (app_message_t*)infotainment_queue_pend(g_test_service_queue, 200, &err);
        if (!msg || err != OS_NO_ERR) {
            // 超时意味着可能所有消息都已收到
            if (g_test_ctx.received_messages >= g_test_ctx.expected_messages) {
                break;
            }
            continue;
        }

        g_test_ctx.received_messages++;

        if (msg->payload) {
            ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
            // 优先级测试逻辑
            if (wrapper->ptr) { // 假设优先级测试的 payload 非空
                switch (msg->command) {
                    case TEST_CMD_PRIORITY_LOW:
                        g_test_ctx.received_low_prio++;
                        if (g_test_ctx.last_received_prio > MSG_PRIO_LOW) g_test_ctx.priority_test_failed = true;
                        g_test_ctx.last_received_prio = MSG_PRIO_LOW;
                        break;
                    case TEST_CMD_PRIORITY_NORMAL:
                        g_test_ctx.received_normal_prio++;
                        if (g_test_ctx.last_received_prio > MSG_PRIO_NORMAL) g_test_ctx.priority_test_failed = true;
                        g_test_ctx.last_received_prio = MSG_PRIO_NORMAL;
                        break;
                    case TEST_CMD_PRIORITY_HIGH:
                        g_test_ctx.received_high_prio++;
                        g_test_ctx.last_received_prio = MSG_PRIO_HIGH;
                        break;
                }
            }            
            free_ref_counted_payload(wrapper);
        }
        infotainment_free(msg);
    }
    
    printf("[BusTest] Test Receiver Service thread finished processing.\n");
    g_test_service_thread = 0; // 标记线程已退出
    infotainment_thread_delete(EXEC_prioself);
}

// --- 测试引擎函数声明 ---
static void run_memory_leak_test(int num_messages, int payload_size);
static void run_priority_test(int num_messages_per_prio);
static void run_stress_test(bus_router_address_t target_address, int num_messages, int payload_size);

// --- 公共API实现 ---

void bus_router_print_memory_snapshot(const char* stage_name) {
    uint32_t free_mem = esMEMS_FreeMemSize();
    printf("[BusTest] --- Memory Snapshot [%s]: Free = %u bytes ---\n", stage_name, free_mem);
    if (strcmp(stage_name, "Before Test") == 0) {
        g_test_ctx.mem_before = free_mem;
    } else {
        g_test_ctx.mem_after = free_mem;
    }
}

void bus_router_start_test(bus_test_mode_t test_mode, service_id_t source_service, service_id_t target_service, int num_messages, int payload_size) {
    printf("\n\n[BusTest] >>>>>>>>>>>>> STARTING BUS ROUTER TEST <<<<<<<<<<<<<<<\n");
    
    memset(&g_test_ctx, 0, sizeof(g_test_ctx));
    if (g_test_service_queue) infotainment_queue_delete(g_test_service_queue, NULL);
    g_test_service_queue = infotainment_queue_create(num_messages * 3 + 10);
    if (!g_test_service_queue) { printf("[BusTest] Test failed: Cannot create test queue.\n"); return; }
    
    bus_router_address_t final_target_address;
    service_registry_entry_t target_info = {0};
    bus_router_get_service_info_by_id(target_service, &target_info);
    
    bool use_internal_receiver = false;
    if (target_info.is_used && test_mode != BUS_TEST_MODE_STRESS_BROADCAST) {
        use_internal_receiver = true;
    }
    
    if (use_internal_receiver) {
        printf("[BusTest] Using internal Test Receiver Service.\n");
        bus_router_register_service(TEST_SERVICE_ID, TEST_SERVICE_ADDRESS, TEST_SERVICE_NAME, g_test_service_queue,NULL,NULL);
        final_target_address = TEST_SERVICE_ADDRESS;
        g_test_service_thread = infotainment_thread_create(test_service_thread_entry, NULL, TASK_PROI_LEVEL2, 1024*4, "TestReceiver");
    } else {
        final_target_address = target_info.is_used ? target_info.address : ADDRESS_BROADCAST;
    }

    switch (test_mode) {
        case BUS_TEST_MODE_MEMORY_LEAK:
            run_memory_leak_test(num_messages, payload_size);
            break;
        case BUS_TEST_MODE_PRIORITY:
            run_priority_test(num_messages); // num_messages 是每个优先级的数量
            break;
        case BUS_TEST_MODE_STRESS_SINGLE:
            run_stress_test(final_target_address, num_messages, payload_size);
            break;
        case BUS_TEST_MODE_STRESS_BROADCAST:
            // 广播到除自己和测试服务外的所有服务
            service_registry_entry_t source_info = {0};
            bus_router_get_service_info_by_id(source_service, &source_info);
            bus_router_address_t broadcast_mask = ADDRESS_BROADCAST & ~(source_info.is_used ? source_info.address : 0) & ~TEST_SERVICE_ADDRESS;
            run_stress_test(broadcast_mask, num_messages, payload_size);
            break;
        case BUS_TEST_MODE_LATENCY:
            BR_INFO("Running Latency Test (Ping-Pong)...");
            start_ping_pong_test(source_service, target_service, "Latency Test", num_messages);
            break;    
        default:
            printf("[BusTest] Test mode %d not implemented.\n", test_mode);
            break;
    }

    if (use_internal_receiver && g_test_service_thread > 0) {
        printf("[BusTest] Waiting for Test Receiver Service to finish...\n");
        g_test_thread_running = false; // 通知线程退出
        infotainment_queue_post(g_test_service_queue, NULL); // 唤醒可能阻塞的线程
        while(g_test_service_thread != 0) { // 等待线程自己清零句柄
            infotainment_thread_sleep(10);
        }
        bus_router_unregister_service(TEST_SERVICE_ID);
        printf("[BusTest] Test Receiver Service cleaned up.\n");
    }
    
    printf("[BusTest] >>>>>>>>>>>>> BUS ROUTER TEST FINISHED <<<<<<<<<<<<<<<\n\n");
}



// --- 各个测试模式的具体实现 ---

/**
 * 1. 内存泄漏测试
 */
static void run_memory_leak_test(int num_messages, int payload_size) {
    BR_INFO("--- Running Memory Leak Test ---");
    BR_INFO("Messages: %d, Payload Size: %d", num_messages, payload_size);

    bus_router_print_memory_snapshot("Before Test");

    for (int i = 0; i < num_messages; i++) {
        void* payload = NULL;
        if (payload_size > 0) {
            payload = infotainment_malloc(payload_size);
            if (payload) memset(payload, i, payload_size);
        }
        // 使用一个不存在的服务作为来源，以便使用通用 free
        bus_post_message(SRC_UNDEFINED, TEST_SERVICE_ADDRESS, MSG_PRIO_NORMAL, TEST_CMD_MEMORY_LEAK, payload, SRC_UNDEFINED, NULL);
    }
    
    // 等待消息处理完成
    infotainment_thread_sleep(500);

    bus_router_print_memory_snapshot("After Test");

    BR_INFO("--- Test Result ---");
    if (g_test_ctx.mem_after == g_test_ctx.mem_before) {
        BR_INFO("Result: PASSED. No memory leak detected.");
    } else {
        BR_ERROR("Result: FAILED. Potential memory leak detected! Delta: %d bytes.", g_test_ctx.mem_before - g_test_ctx.mem_after);
    }
    BR_INFO("---------------------");
}

/**
 * 2. 优先级测试
 */
static void run_priority_test(int num_messages_per_prio) {
    BR_INFO("--- Running Priority Test ---");
    BR_INFO("Messages per priority: %d", num_messages_per_prio);
    g_test_ctx.last_received_prio = MSG_PRIO_CRITICAL + 1; // 初始为一个不可能的值

    // 按从低到高的顺序发送消息
    for (int i = 0; i < num_messages_per_prio; i++) {
        bus_post_message(SRC_UNDEFINED, TEST_SERVICE_ADDRESS, MSG_PRIO_LOW, TEST_CMD_PRIORITY_LOW, (void*)1, SRC_UNDEFINED, NULL);
    }
    for (int i = 0; i < num_messages_per_prio; i++) {
        bus_post_message(SRC_UNDEFINED, TEST_SERVICE_ADDRESS, MSG_PRIO_NORMAL, TEST_CMD_PRIORITY_NORMAL, (void*)1, SRC_UNDEFINED, NULL);
    }
    for (int i = 0; i < num_messages_per_prio; i++) {
        bus_post_message(SRC_UNDEFINED, TEST_SERVICE_ADDRESS, MSG_PRIO_HIGH, TEST_CMD_PRIORITY_HIGH, (void*)1, SRC_UNDEFINED, NULL);
    }

    infotainment_thread_sleep(500); // 等待处理

    BR_INFO("--- Test Result ---");
    BR_INFO("Sent (High/Normal/Low): %d / %d / %d", num_messages_per_prio, num_messages_per_prio, num_messages_per_prio);
    BR_INFO("Received (High/Normal/Low): %d / %d / %d", g_test_ctx.received_high_prio, g_test_ctx.received_normal_prio, g_test_ctx.received_low_prio);
    
    if (g_test_ctx.priority_test_failed) {
        BR_ERROR("Result: FAILED. Messages were received out of priority order.");
    } else if (g_test_ctx.received_high_prio == num_messages_per_prio &&
               g_test_ctx.received_normal_prio == num_messages_per_prio &&
               g_test_ctx.received_low_prio == num_messages_per_prio) {
        BR_INFO("Result: PASSED. All messages received in correct priority order.");
    } else {
        BR_ERROR("Result: FAILED. Message loss detected.");
    }
    BR_INFO("---------------------");
}

/**
 * 3. 抗压能力测试
 */
static void run_stress_test(bus_router_address_t target_address, int num_messages, int payload_size) {
    BR_INFO("--- Running Stress Test ---");
    BR_INFO("Target Address: 0x%llX, Messages: %d, Payload Size: %d", target_address, num_messages, payload_size);
    g_test_ctx.expected_messages = num_messages;

    for (int i = 0; i < num_messages; i++) {
        void* payload = NULL;
        if (payload_size > 0) {
            payload = infotainment_malloc(payload_size);
        }
        bus_post_message(SRC_UNDEFINED, target_address, MSG_PRIO_NORMAL, TEST_CMD_STRESS, payload, SRC_UNDEFINED, NULL);
    }

    infotainment_thread_sleep(500); // 等待处理

    BR_INFO("--- Test Result ---");
    BR_INFO("Expected Messages: %d", g_test_ctx.expected_messages);
    BR_INFO("Received Messages: %d", g_test_ctx.received_messages);
    
    if (g_test_ctx.received_messages == g_test_ctx.expected_messages) {
        BR_INFO("Result: PASSED. No message loss detected.");
    } else {
        BR_ERROR("Result: FAILED. Message loss detected! Lost: %d", g_test_ctx.expected_messages - g_test_ctx.received_messages);
    }
    BR_INFO("---------------------");
}

#else // ENABLE_BUS_TESTING is not defined

// 如果测试宏未定义，则提供空的函数桩，以避免链接错误
#include "bus_router_test.h"
#include <stdio.h>

void bus_router_start_test(bus_test_mode_t test_mode, service_id_t source_service, service_id_t target_service, int num_messages, int payload_size) {
    printf("Bus testing is disabled in this build.\n");
}

void bus_router_print_memory_snapshot(const char* stage_name) {
    printf("Bus testing is disabled in this build.\n");
}

#endif // ENABLE_BUS_TESTING