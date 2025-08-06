#ifndef INFOTAINMENT_OS_API_H
#define INFOTAINMENT_OS_API_H


#include <typedef.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kapi.h>
#include <desktop_api.h>


#ifdef __cplusplus
extern "C" {
#endif
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_PREFIX, SYS_LOG_COLOR_PURPLE, ##__VA_ARGS__); } while(0)
#define __inf(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_INFO, LOG_LEVEL_INFO_PREFIX, SYS_LOG_COLOR_BLUE, ##__VA_ARGS__); } while(0)
#define __msg(...)    do { LOG_NO_COLOR(OPTION_LOG_LEVEL_MESSAGE, LOG_LEVEL_VERBOSE_PREFIX, ##__VA_ARGS__); } while(0)



#define TASK_PROI_LEVEL1          KRNL_priolevel1
#define TASK_PROI_LEVEL2          0x7D
#define TASK_PROI_LEVEL3          0x7E
#define TASK_PROI_LEVEL4          0x7F
#define TASK_PROI_LEVEL5         KRNL_priolevel3


// 队列（消息队列）
#define infotainment_queue_create(max_msgs)                         esKRNL_QCreate(max_msgs)
#define infotainment_queue_post(queue, msg)                         esKRNL_QPost(queue, msg)
#define infotainment_queue_pend(queue, timeout, err)                esKRNL_QPend(queue, timeout, err)
#define infotainment_queue_delete(queue,err)                        esKRNL_QDel(queue, OS_DEL_ALWAYS,err)
#define infotainment_queue_flush(queue)                             esKRNL_QFlush(queue)
#define infotainment_queue_query(queue,p_q_data)                    esKRNL_QQuery(queue, p_q_data)
#define infotainment_queue_get(queue,err)                    esKRNL_QAccept(queue, err)




// 邮箱
#define infotainment_mailbox_create(max_msgs)              esKRNL_MboxCreate(max_msgs)
#define infotainment_mailbox_post(mbox, msg)               esKRNL_MboxPost(mbox, msg)
#define infotainment_mailbox_pend(mbox, timeout, err)      esKRNL_MboxPend(mbox, timeout, err)
#define infotainment_mailbox_delete(mbox,err)                  esKRNL_MboxDel(mbox, OS_DEL_ALWAYS, err)

// 任务/线程
static inline unsigned int infotainment_thread_create(void (*entry)(void *p_arg), void *arg, uint16_t prio, uint32_t stack_size, const char *name) {
    unsigned int hd = -1;
    hd = esKRNL_TCreate(entry, arg, stack_size, prio);
    if (hd == -1) {
        return hd; // 创建失败
    }
    esKRNL_TaskNameSet(hd, name);
    return hd; // 成功返回线程句柄
    
}

static inline int infotainment_thread_delete_req(unsigned int thread) {
    if (thread <= 0) {
        return OS_TASK_NOT_EXIST; // 无效线程句柄
    }
    
    return esKRNL_TDelReq(thread);
}

static inline int infotainment_thread_TDelReq(unsigned int thread) {
    if (thread == 0) {
        return EPDK_FALSE; // 无效线程句柄
    }
    if (esKRNL_TDelReq(thread) == OS_TASK_DEL_REQ)
    {
       return EPDK_TRUE;
    }
    return EPDK_FALSE; 
}

static inline int infotainment_thread_delete(unsigned int thread) {
    if (thread == 0) {
        return -1; // 无效线程句柄
    }
    esKRNL_TDel(thread);
   
}



// 信号量
#define infotainment_sem_create(initial_count)             esKRNL_SemCreate(initial_count)
#define infotainment_sem_wait(sem, timeout,err)           esKRNL_SemPend(sem, timeout,err)
#define infotainment_sem_post(sem)                        esKRNL_SemPost(sem)
#define infotainment_sem_delete(sem,err)                  esKRNL_SemDel(sem, OS_DEL_ALWAYS,err)

// 互斥锁
#define infotainment_mutex_create(prio, err)              esKRNL_MutexCreate(prio, err)
#define infotainment_mutex_delete(mutex, err)        esKRNL_MutexDel(mutex, OS_DEL_ALWAYS, err)
#define infotainment_mutex_pend(mutex, timeout, err)      esKRNL_MutexPend(mutex, timeout, err)
#define infotainment_mutex_post(mutex)                    esKRNL_MutexPost(mutex)

// 内存
#define MEM_DEBUG_ENABLE 0// <--- 在这里控制总开关，1为开启，0为关闭

#if MEM_DEBUG_ENABLE
// 内存哨兵的头部结构体
typedef struct {
    uint32_t magic_start;   // 头部魔法数字
    size_t   size;          // 用户请求的原始大小
    const char* file;       // 分配位置的文件名
    int      line;          // 分配位置的行号
} MemGuardHeader;

#define MEM_GUARD_MAGIC_START 0xDEADBEEF
#define MEM_GUARD_MAGIC_END   0xCAFEF00D

// 这是一个简化的、非线程安全的 malloc 实现，用于调试
// 在您的系统中，请替换为实际的内存分配函数 e.g., esMEMS_Malloc
static void* _infotainment_malloc_debug(size_t size, const char* file, const char* func, int line) {
    // 计算总分配大小：头部 + 用户数据 + 尾部
    size_t total_size = sizeof(MemGuardHeader) + size + sizeof(uint32_t);
    void* block = esMEMS_Malloc(0, total_size);
    if (!block) {
        printf("[MEM_GUARD_ERROR] Malloc FAILED for %d bytes at %s:%d\n", size, file, line);
        return NULL;
    }

    // 填充头部信息
    MemGuardHeader* header = (MemGuardHeader*)block;
    header->magic_start = MEM_GUARD_MAGIC_START;
    header->size = size;
    header->file = file;
    header->line = line;

    // 填充尾部哨兵
    uint32_t* footer = (uint32_t*)((char*)block + sizeof(MemGuardHeader) + size);
    *footer = MEM_GUARD_MAGIC_END;

    // 返回给用户的指针是头部之后的数据区
    void* user_ptr = (char*)block + sizeof(MemGuardHeader);
    
    printf("[MEM_TRACE] Malloc: UserPtr %p (%d bytes) at %s:%d\n", user_ptr, size, file, line);
    return user_ptr;
}


// 这是一个简化的、非线程安全的 free 实现，用于调试
static void _infotainment_free_debug(void* ptr, const char* file, const char* func, int line) {
    if (!ptr) return;

    // 根据用户指针计算出整个内存块的起始地址
    void* block = (char*)ptr - sizeof(MemGuardHeader);
    MemGuardHeader* header = (MemGuardHeader*)block;

    bool corruption = false;

    // 1. 检查头部哨兵
    if (header->magic_start != MEM_GUARD_MAGIC_START) {
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("[MEM_GUARD_ERROR] CORRUPTION DETECTED at %s:%d!\n", file, line);
        printf("  -> Block allocated at %s:%d has a CORRUPTED HEADER (Buffer Underflow?).\n", header->file, header->line);
        printf("  -> Expected Header Magic: 0x%08X, Found: 0x%08X\n", MEM_GUARD_MAGIC_START, header->magic_start);
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        corruption = true;
    }

    // 2. 检查尾部哨兵
    uint32_t* footer = (uint32_t*)((char*)ptr + header->size);
    if (*footer != MEM_GUARD_MAGIC_END) {
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("[MEM_GUARD_ERROR] CORRUPTION DETECTED at %s:%d!\n", file, line);
        printf("  -> Block allocated at %s:%d has a CORRUPTED FOOTER (BUFFER OVERFLOW!).\n", header->file, header->line);
        printf("  -> Expected Footer Magic: 0x%08X, Found: 0x%08X\n", MEM_GUARD_MAGIC_END, *footer);
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        corruption = true;
    }

    if (corruption) {
        // 如果发生腐败，可以选择不释放内存，让系统崩溃，以便调试器能捕获到现场
        // 或者打印信息后继续释放，可能会引发后续的 chunk 错误
        // 这里我们选择打印信息后继续，以符合您看到的现象
    }

    printf("[MEM_TRACE] Free:   UserPtr %p at %s:%d\n", ptr, file, line);
    esMEMS_Mfree(0, block); // 释放整个内存块
}


// 重新定义 malloc 和 free，让它们自动打印日志
#define infotainment_malloc(size) \
    _infotainment_malloc_debug(size, __FILE__, __FUNCTION__, __LINE__)

#define infotainment_free(ptr) \
    _infotainment_free_debug(ptr, __FILE__, __FUNCTION__, __LINE__)

#else
#define infotainment_malloc(size)                         esMEMS_Malloc(0, size)
#define infotainment_free(ptr)                             esMEMS_Mfree(0, ptr)                                                           
#endif // MEM_DEBUG_ENABLE

                                                                           
//#define infotainment_calloc(n, size)                      infotainment_malloc((n)*(size))
//#define infotainment_realloc(ptr, size)                   infotainment_malloc(size) // melis无realloc，需自实现

#define infotainment_thread_sleep(ticks)                  esKRNL_TimeDly(ticks)
#define infotainment_get_ticks()                           esKRNL_TimeGet() // 获取系统滴答计数
#ifdef __cplusplus
}
#endif

#endif // INFOTAINMENT_OS_API_H
