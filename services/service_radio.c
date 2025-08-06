#include "service_radio.h" // 包含Radio服务自己的头文件
#include "bus_router.h"      // 包含消息总线路由器
#include "infotainment_os_api.h" // 包含OS相关的函数，如线程、队列、内存管理
#include "radio_api.h"
#include <elibs_stdio.h>
#include <radio/radio_hal.h>

// 使用我们之前定义的枚举和结构体
// (假设这些定义在 service_radio.h 或其他全局头文件中)
// #include "radio_message_defs.h"

#define printf eLIBs_printf
static const char* radio_state_to_string(radio_state_t state);
static void radio_timer_stop(radio_internal_timer_type_t timer_type) ;
static void radio_exit_state(radio_state_t old_state);
static void radio_enter_state(radio_state_t new_state);
// --- 调试功能相关定义 ---
static int g_radio_service_debug = 1;  // 调试开关，默认关闭
static int g_radio_service_info = 1;   // 信息开关，默认关闭

static const char* const fm_res_rds_pty_string[32] =
{
	" ",//"No PTY",//0
	"News",
	"Current Affairs",
	"Information",
	"Sport",
	"Education",
	"Drama",
	"Cultures",
	"Science",
	"Varied Speech",
	"Pop Music",
	"Rock Music",
	"Easy Listening",
	"Light Classics M",
	"Serious Classics",
	"Other Music",
	"Weather&Metr",
	"Finance",
	"Children's Progs",
	"Social Affairs",
	"Religion",
	"Phone In",
	"Travel&Touring",
	"Leisure&Hobby",
	"Jazz Music",
	"Country Music",
	"National Music",
	"Oldies Music",
	"Folk Music",
	"Documentary",
	"Alarm Test",
	"Alarm-Alarm!"
};


static const char* const fm_res_rbds_pty_string[32] =
{
	" ",//"No PTY",//0
	"News",
	"Information",
	"Sports",
	"Talk",
	"Rock",
	"Classic_Rock",
	"Adult_Hits",
	"Soft_Rock",
	"Top_40",
	"Country",
	"Oldies",
	"Soft",
	"Nostalgia",
	"Jazz",
	"Classical",
	"Rhythm_and_Blues",
	"Soft_R_&_B",
	"Foreign_Language",
	"Religious_Music",
	"Religious_Talk",
	"Personality",
	"Public",
	"College",
	"Hablar_Espanol",
	"Musica_Espanol",
	"Hip hop",
	" ",
	" ",
	"Weather",
	"Emergency_Test",
	"ALERT!_ALERT!",
};




// 调试信息打印宏 - 添加行号显示
#define RADIO_DEBUG(fmt, ...) \
    do { \
        if (g_radio_service_debug) { \
            printf("[RadioService-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define RADIO_INFO(fmt, ...) \
    do { \
        if (g_radio_service_info) { \
            printf("[RadioService-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define RADIO_ERROR(fmt, ...) \
    printf("[RadioService-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)

// --- 静态变量 ---
static void* service_radio_queue_handle = 0;
static int service_radio_thread_handle = 0;
#define RADIO_QUEUE_SIZE 256
static int service_radio_payload_use_count = 0;
static void* service_radio_semaphore = NULL;

// 【新增】定义一个结构体来保存单个定时器的信息
typedef struct {
    radio_internal_timer_type_t type;     // 定时器的用途
    uint32_t                    timer_id; // 从 service_timer 返回的ID
} active_timer_info_t;

// 【新增】Radio 服务上下文结构体
typedef struct {
    radio_state_t current_state; // 当前状态
    radio_state_payload_t radio_data; // 保存当前电台的所有信息
    const reg_fm_para_t	   *radio_reg_para;//服务注册表内存
    const reg_init_para_t	   *init_reg_para;//服务注册表内存

    // 使用一个固定大小的数组来管理所有活动定时器
    // RADIO_TIMER_TYPE_COUNT 保证了数组大小足够
    active_timer_info_t active_timers[RADIO_TIMER_TYPE_COUNT];
} radio_service_context_t;

static radio_service_context_t g_radio_ctx; // 全局服务上下文实例



// --- 外部控制调试开关的函数 ---
void service_radio_set_debug(int enable) {
    g_radio_service_debug = enable;
    RADIO_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void service_radio_set_info(int enable) {
    g_radio_service_info = enable;
    RADIO_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

// --- 函数声明 ---
static void radio_thread_entry(void* p_arg);
void radio_handle_ping_request(app_message_t* msg);
void radio_handle_pong_response(app_message_t* msg);
void radio_send_ping_to_system(void);
/**
 * @brief 分配用于Radio服务的消息载荷内存
 * @return 成功则返回指向载荷的指针，失败返回NULL
 */
radio_state_payload_t *service_radio_payload_alloc(void) {
    radio_state_payload_t* service_radio_payload = NULL;
    unsigned char err = 0;

    if (service_radio_semaphore)
        infotainment_sem_wait(service_radio_semaphore, 0, &err);

    // 简单的保护机制，防止已分配的载荷超过队列容量
    if (service_radio_payload_use_count >= RADIO_QUEUE_SIZE) {
        if (service_radio_semaphore)
            infotainment_sem_post(service_radio_semaphore);
        RADIO_ERROR("Payload allocation limit reached");
        return NULL;
    }

    service_radio_payload = (radio_state_payload_t*)infotainment_malloc(sizeof(radio_state_payload_t));
    if (!service_radio_payload) {
        RADIO_ERROR("Failed to allocate payload memory");
        if (service_radio_semaphore)
            infotainment_sem_post(service_radio_semaphore);
        return NULL; // 内存分配失败
    }
    
    service_radio_payload_use_count++;

    if (service_radio_semaphore)
        infotainment_sem_post(service_radio_semaphore);

    return service_radio_payload;
}

/**
 * @brief 释放由 service_radio_payload_alloc 分配的内存
 * @param payload 指向要释放的载荷的指针
 */
void service_radio_payload_free(radio_state_payload_t* payload) {
    unsigned char err = 0;

    if (service_radio_semaphore)
        infotainment_sem_wait(service_radio_semaphore, 0, &err);

    if (payload) {
        infotainment_free(payload);
        if (service_radio_payload_use_count > 0) {
            service_radio_payload_use_count--;
        }
    }

    if (service_radio_semaphore)
        infotainment_sem_post(service_radio_semaphore);
}

radio_state_payload_t* service_radio_payload_copy(const radio_state_payload_t* src) {
    if (!src) return NULL;
    radio_state_payload_t* dst = service_radio_payload_alloc();
    if (dst) {
        memcpy(dst, src, sizeof(radio_state_payload_t));
    }
    return dst;
}


void service_radio_update_payload_data(void)
{
    if(g_radio_ctx.radio_reg_para)
    {
        g_radio_ctx.radio_data.frequency = g_radio_ctx.radio_reg_para->cur_freq;
        g_radio_ctx.radio_data.max_freq = g_radio_ctx.radio_reg_para->cur_max_freq;
        g_radio_ctx.radio_data.min_freq = g_radio_ctx.radio_reg_para->cur_min_freq;
        g_radio_ctx.radio_data.tu_step = g_radio_ctx.radio_reg_para->cur_tu_step;
        g_radio_ctx.radio_data.band = g_radio_ctx.radio_reg_para->cur_band;
        g_radio_ctx.radio_data.pty = g_radio_ctx.radio_reg_para->cur_pty_type;
        g_radio_ctx.radio_data.pty_seek_type = g_radio_ctx.init_reg_para->pty_seek;
        g_radio_ctx.radio_data.rds_on = g_radio_ctx.init_reg_para->rds_on_off;
        g_radio_ctx.radio_data.af_on = g_radio_ctx.init_reg_para->af_on_off;
        g_radio_ctx.radio_data.ta_on = g_radio_ctx.init_reg_para->ta_on_off;
        g_radio_ctx.radio_data.tp_on = g_radio_ctx.init_reg_para->tp_on_off;
        g_radio_ctx.radio_data.ct_on = g_radio_ctx.init_reg_para->ct_on_off;
        g_radio_ctx.radio_data.local_seek_on = g_radio_ctx.init_reg_para->local_seek_on_off;
        g_radio_ctx.radio_data.mono_on = g_radio_ctx.init_reg_para->mono_on_off;
        g_radio_ctx.radio_data.area = g_radio_ctx.init_reg_para->area;
        g_radio_ctx.radio_data.seek_type = g_radio_ctx.radio_reg_para->seek_type;

        // 设置当前频率字符串
        if(g_radio_ctx.init_reg_para->area_type == TUNER_TYPE_K)
        {
            g_radio_ctx.radio_data.tuner_area[0] = RADIO_LATAM1_BAND;
            g_radio_ctx.radio_data.tuner_area[1] = RADIO_USA1_BAND;
            g_radio_ctx.radio_data.tuner_area[2] = RADIO_EUROPE1_BAND;
            g_radio_ctx.radio_data.tuner_area[3] = RADIO_ASIA_BAND;
            g_radio_ctx.radio_data.tuner_area[4] = RADIO_MID_EAST_BAND;
            g_radio_ctx.radio_data.tuner_area[5] = RADIO_OCEANIA_BAND;
            g_radio_ctx.radio_data.tuner_area_count = 6;
        }
        else if(g_radio_ctx.init_reg_para->area_type == TUNER_TYPE_M)
        {
            g_radio_ctx.radio_data.tuner_area[0] = RADIO_LATAM1_BAND;
            g_radio_ctx.radio_data.tuner_area[1] = RADIO_USA1_BAND;
            g_radio_ctx.radio_data.tuner_area[2] = RADIO_EUROPE1_BAND;
            g_radio_ctx.radio_data.tuner_area[3] = RADIO_ASIA_BAND;
            g_radio_ctx.radio_data.tuner_area[4] = RADIO_MID_EAST_BAND;
            g_radio_ctx.radio_data.tuner_area[5] = RADIO_OCEANIA_BAND;
            g_radio_ctx.radio_data.tuner_area_count = 6;
        }
        else if(g_radio_ctx.init_reg_para->area_type == TUNER_TYPE_E)
        {
            g_radio_ctx.radio_data.tuner_area_count = 0; // 假设欧洲地区没有特殊的区域设置
        }



        if(g_radio_ctx.radio_reg_para->cur_preset>=0 && g_radio_ctx.radio_reg_para->cur_preset < RADIO_MAX_PRESET)
        {
        #ifdef NO_FM3_AM2_BAND
            g_radio_ctx.radio_data.preset_index = g_radio_ctx.radio_reg_para->cur_preset;
        #else
            g_radio_ctx.radio_data.preset_index = g_radio_ctx.radio_reg_para->cur_preset%6;
        #endif
        }
        else	
        {
            g_radio_ctx.radio_data.preset_index = 0; // 如果当前预设索引无效，设置为0
        }

        // 1. 复制 curFreq_Band 数组 (一维数组)
        memcpy(g_radio_ctx.radio_data.curFreq_Band, 
            g_radio_ctx.radio_reg_para->curFreq_Band, 
            sizeof(g_radio_ctx.radio_data.curFreq_Band));

        // 2. 复制 FM1_3_AM1_2_freq 数组 (二维数组)
        memcpy(g_radio_ctx.radio_data.FM1_3_AM1_2_freq, 
            g_radio_ctx.radio_reg_para->FM1_3_AM1_2_freq, 
            sizeof(g_radio_ctx.radio_data.FM1_3_AM1_2_freq));

        // 3. 复制 FM1_3_AM1_2_freq_un 数组 (二维数组)
        memcpy(g_radio_ctx.radio_data.FM1_3_AM1_2_freq_un, 
            g_radio_ctx.radio_reg_para->FM1_3_AM1_2_freq_un, 
            sizeof(g_radio_ctx.radio_data.FM1_3_AM1_2_freq_un));


        strncpy(g_radio_ctx.radio_data.ps_name, g_radio_ctx.radio_reg_para->ps_data, sizeof(g_radio_ctx.radio_data.ps_name) - 1);
        g_radio_ctx.radio_data.ps_name[sizeof(g_radio_ctx.radio_data.ps_name) - 1] = '\0';

    }
    else
    {
        g_radio_ctx.radio_data.frequency = 87500;
        strncpy(g_radio_ctx.radio_data.band_str, "FM1", sizeof(g_radio_ctx.radio_data.band_str) - 1);
    }

}

static int radio_setup_event(radio_setup_payload_t* payload) {
    if (!payload) {
        RADIO_ERROR("Payload is NULL, cannot setup event");
        return -1;
    }

    RADIO_INFO("Setting up event: type=%d, value=%d", payload->setup_type, payload->setting_value);
    switch (payload->setup_type) {
        case RADIO_SETUP_SEEK_TYPE:
            // 设置搜台类型
            if(payload->setting_value < AUTO_SEEK || payload->setting_value >= MAX_SEEK_TYPE) {
                RADIO_ERROR("Invalid seek type: %d", payload->setting_value);
                return -1; // 无效的搜台类型
            }
            radio_hal_set_seek_type(payload->setting_value);
            // 更新服务上下文中的搜台类型
            // 这里可以添加其他相关逻辑，如更新UI或通知其他模块
            // 例如：g_radio_ctx.radio_reg_para->seek_type = payload->setting_value;
            // 或者通过消息总线发送一个更新事件
           // g_radio_ctx.radio_reg_para->seek_type = payload->setting_value;
            RADIO_INFO("Seek type set to %d", payload->setting_value);
            break;
        case RADIO_SETUP_AREA:
            radio_hal_set_area(payload->setting_value);
            RADIO_INFO("Area set to %d", payload->setting_value);
            break;
        case RADIO_SETUP_RDS_ON_OFF:
            radio_hal_set_rds_enabled(payload->setting_value ? true : false);
            RADIO_INFO("RDS set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_AF_ON_OFF:
            radio_hal_set_rds_af_enabled(payload->setting_value ? true : false);
            RADIO_INFO("AF set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_TA_ON_OFF:
            radio_hal_set_rds_ta_enabled(payload->setting_value ? true : false);
            RADIO_INFO("TA set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_TP_ON_OFF:
            RADIO_INFO("TP set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_CT_ON_OFF:
            radio_hal_set_rds_ct_enabled(payload->setting_value ? true : false);
            RADIO_INFO("CT set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_LOC_ON_OFF:
            radio_hal_set_loc_dx_mode(payload->setting_value ? true : false);
            RADIO_INFO("Local seek set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        case RADIO_SETUP_MONO_ON_OFF:
            RADIO_INFO("Mono set to %s", payload->setting_value ? "ON" : "OFF");
            break;
        default:
            RADIO_ERROR("Unknown setup type: %d", payload->setup_type);
            return -1; // 未知的设置类型
    }
    return 0;
}

static int radio_seek_type_event(char next) 
{
    // 这里可以根据当前的搜台类型执行不同的逻辑
    // 例如，如果是自动搜台，则启动一个定时器来更新UI
    // 如果是手动搜台，则可能需要等待用户输入
    RADIO_INFO("Current seek type: %d", g_radio_ctx.radio_reg_para->seek_type);
    
    if(g_radio_ctx.current_state != RADIO_STATE_PLAYING) {
        RADIO_ERROR("Cannot change seek type while in state: %s", radio_state_to_string(g_radio_ctx.current_state));
        return -1; // 不能在非空闲状态下更改搜台类型
    }


    // 假设我们有一个函数来处理不同的搜台类型
    switch (g_radio_ctx.radio_reg_para->seek_type) {
        case AUTO_SEEK:
            // 启动自动搜台逻辑
            radio_exit_state(g_radio_ctx.current_state); // 退出当前状态
            if(next) {
                radio_hal_seek_down(); // 向下搜台
            } else {
                radio_hal_seek_up(); // 向上搜台
            }
            radio_enter_state(RADIO_STATE_SEEKING);

            break;
        case MANUAL_SEEK:
            // 启动手动搜台逻辑
             // 启动自动搜台逻辑
            radio_exit_state(g_radio_ctx.current_state); // 退出当前状态
            if(next) {
                radio_hal_tune_down(); // 向下搜台
            } else {
                radio_hal_tune_up(); // 向上搜台
            }
            radio_enter_state(RADIO_STATE_TUNING);
            break;

        case PERSET_SEEK:
        {
            char temp_preset_index = g_radio_ctx.radio_data.preset_index;

            if(next) {
                // 向下搜台
                temp_preset_index--;
                if(temp_preset_index < 0) {
                    temp_preset_index = RADIO_MAX_CH - 1; // 如果小于0，设置为最大预设频道
                }
            } else {
                // 向上搜台
                temp_preset_index++;
                if(temp_preset_index >= RADIO_MAX_CH) {
                    temp_preset_index = 0; // 如果超过最大预设频道，重置为0
                }
            }

            // 启动预设搜台逻辑
            radio_exit_state(g_radio_ctx.current_state);
            radio_hal_play_preset(temp_preset_index+1);
            radio_enter_state(RADIO_STATE_PLAY_PRESET);
        }
            break;

            
        default:
            RADIO_ERROR("Unsupported seek type: %d", g_radio_ctx.radio_reg_para->seek_type);
            return -1; // 不支持的搜台类型
    }
    
    return 0; // 成功
}






static int service_radio_find_null_slot_preset_list(void)
{
    char cur_band = g_radio_ctx.radio_reg_para->cur_band;
    int i=0;
    int max_preset = RADIO_MAX_CH;



    for(i=0;i<max_preset;i++)
    {
        if(g_radio_ctx.radio_reg_para->FM1_3_AM1_2_freq_un[cur_band][i] != 0xFFFF)
        {
            // 如果当前频率不为0xFFFF，表示该位置已被使用
            continue;
        }
        else
        {
            // 找到一个空位
            return i + 1; // 返回1开始的索引
        }

   
    }
	

    return 1;//覆盖第一个
}



// File: service_radio.c

// --- 新增定时器管理辅助函数 ---

/**
 * @brief 启动一个内部定时器，并自动管理其ID
 * @param timer_type 我们定义的内部定时器类型
 * @param ...      timer_start 所需的其他参数
 */
static void radio_timer_start(
    radio_internal_timer_type_t timer_type,
    uint32_t duration_ms,
    timer_type_t type,
    unsigned int target_command,
    void* user_data) 
{
    // 在启动新定时器前，先确保同类型的旧定时器已被停止
    radio_timer_stop(timer_type);

    uint32_t new_id = timer_start(
        duration_ms,
        type,
        ADDRESS_RADIO_SERVICE,
        target_command,
        user_data,
        user_data ? SRC_RADIO_SERVICE : SRC_UNDEFINED // 假设 payload 都是 radio 自己创建的
    );
    
    if (new_id != TIMER_SERVICE_INVALID_ID) {
        // 成功启动，将ID保存到我们的管理数组中
        for (int i = 0; i < RADIO_TIMER_TYPE_COUNT; i++) {
            if (g_radio_ctx.active_timers[i].type == RADIO_TIMER_NONE) {
                g_radio_ctx.active_timers[i].type = timer_type;
                g_radio_ctx.active_timers[i].timer_id = new_id;
                RADIO_DEBUG("Internal timer started: type=%d, id=%u", timer_type, new_id);
                return;
            }
        }
        // 如果数组满了（理论上不应该发生），停止刚刚创建的定时器
        RADIO_ERROR("Active timers array is full! Stopping orphaned timer %u.", new_id);
        timer_stop(new_id);
    }
}

/**
 * @brief 停止一个内部定时器
 * @param timer_type 我们定义的内部定时器类型
 */
static void radio_timer_stop(radio_internal_timer_type_t timer_type) {
    for (int i = 0; i < RADIO_TIMER_TYPE_COUNT; i++) {
        if (g_radio_ctx.active_timers[i].type == timer_type) {
            if (g_radio_ctx.active_timers[i].timer_id != TIMER_SERVICE_INVALID_ID) {
                timer_stop(g_radio_ctx.active_timers[i].timer_id);
                RADIO_DEBUG("Internal timer stopped: type=%d, id=%u", timer_type, g_radio_ctx.active_timers[i].timer_id);
            }
            // 从管理数组中移除
            g_radio_ctx.active_timers[i].type = RADIO_TIMER_NONE;
            g_radio_ctx.active_timers[i].timer_id = TIMER_SERVICE_INVALID_ID;
            return;
        }
    }
}

/**
 * @brief 停止所有由本服务启动的定时器 (在退出状态时调用)
 */
static void radio_timer_stop_all(void) {
    for (int i = 0; i < RADIO_TIMER_TYPE_COUNT; i++) {
        if (g_radio_ctx.active_timers[i].type != RADIO_TIMER_NONE) {
            radio_timer_stop(g_radio_ctx.active_timers[i].type);
        }
    }
}



/**
 * @brief 进入新状态时要执行的动作
 */
static void radio_enter_state(radio_state_t new_state) {
    RADIO_INFO("Entering state: %s", radio_state_to_string(new_state));
    if (new_state == g_radio_ctx.current_state) {
        RADIO_ERROR("Already in state: %s", radio_state_to_string(new_state));
        return;
    }
    // 更新全局状态
    g_radio_ctx.current_state = new_state;

    switch (new_state) {
        case RADIO_STATE_INIT:
            // 等待FM模块初始化完成
            radio_timer_start(RADIO_TIMER_CHECK_HARDWARE_STATUS, 100, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_CHECK_HARDWARE_STATUS, NULL);
            break;

        case RADIO_STATE_SEEKING:
            // 进入搜台状态时，启动一个超时定时器
            radio_timer_start(RADIO_TIMER_SEEK_UPDATE_UI, 60, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT, NULL);
            // 向UI发送“正在搜索...”的通知
            service_radio_update_payload_data();
            g_radio_ctx.radio_data.cur_seeking_type = CUR_AUTO_SEEK;
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_STARTED, payload, SRC_RADIO_SERVICE, NULL);
            break;

        case RADIO_STATE_PTY_SEEK:
        {
            // 进入搜台状态时，启动一个超时定时器
            radio_timer_start(RADIO_TIMER_SEEK_UPDATE_UI, 60, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT, NULL);
            // 向UI发送“正在搜索...”的通知
            service_radio_update_payload_data();
            g_radio_ctx.radio_data.cur_seeking_type = CUR_PTY_SEEK;
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_STARTED, payload, SRC_RADIO_SERVICE, NULL);
        }
            break;    
        
        case RADIO_STATE_PLAYING:
            // 进入播放状态，通知UI搜索结束
            radio_timer_start(RADIO_TIMER_PLAYING_UPDATE_UI, 200, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_PLAYING_UPDATE_UI, NULL);
            //bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_FINISHED, NULL, SRC_UNDEFINED, NULL);
            break;

        case RADIO_STATE_TUNING:
            // 进入微调状态时，启动一个步进定时器
            radio_timer_start(RADIO_TIMER_TUNE_STATUS, 2000, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_TUNE_STATUS, NULL);
            radio_timer_start(RADIO_TIMER_TUNE_STEP, 100, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_TUNE_STEP, NULL);
            break;
        case RADIO_STATE_AUTO_STORING:
        {
            // 进入搜台状态时，启动一个超时定时器
            radio_timer_start(RADIO_TIMER_SEEK_UPDATE_UI, 60, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT, NULL);
            // 向UI发送“正在搜索...”的通知
            service_radio_update_payload_data();
            g_radio_ctx.radio_data.cur_seeking_type = CUR_AUTOSTORE_SEEK;
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_STARTED, payload, SRC_RADIO_SERVICE, NULL);
            break;
        }

        case RADIO_STATE_SAVE_PRESET:
        {
            radio_timer_start(RADIO_TIMER_PRESET_SAVE_STATUS, 100, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_PRESET_SAVE_STATUS, NULL);
        }
        break;

        case RADIO_STATE_PLAY_PRESET:
        {
            radio_timer_start(RADIO_TIMER_PRESET_PLAY_STATUS, 100, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_PRESET_PLAY_STATUS, NULL);
        }
        break;

        case RADIO_STATE_SWITCH_BAND:
        {
            // 进入切换波段状态时，通知UI波段切换
            radio_timer_start(RADIO_TIMER_SWITCH_BAND_STATUS, 2000, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_SWITCH_BAND_STATUS, NULL);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SWITCH_BAND_STARTED, NULL, SRC_UNDEFINED, NULL);
            break;
        }

        case RADIO_STATE_SWITCH_BAND_AND_TUNE:
            // 进入切换波段并微调状态时，通知UI波段切换
            radio_timer_start(RADIO_TIMER_SWITCH_BAND_AND_TUNE_STATUS, 2000, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_SWITCH_BAND_AND_TUNE_STATUS, NULL);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SWITCH_BAND_STARTED, NULL, SRC_UNDEFINED, NULL);
            break;

        case RADIO_STATE_RDS:
        {
            force_request_payload_t* req = (force_request_payload_t*)infotainment_malloc(sizeof(force_request_payload_t));
            if(req) {
                req->intent.widget_id = MANGER_ID_SOURCE;
                req->intent.source_type = UI_SOURCE_TYPE_FM;
                req->media_intent = MEDIA_INTENT_TRAFFIC_ANNOUNCEMENT; // 启用RDS
                bus_post_message(SRC_RADIO_SERVICE, ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL, SYSTEM_CMD_FORCE_REQUEST_DISPLAY, req, SRC_UNDEFINED, NULL);
            } else {
                RADIO_ERROR("Failed to allocate memory for force request payload");
            }
        }
        break;    
       
        
        // ... 其他状态的进入动作 ...
        default:
            break;
    }
}


/**
 * @brief 退出旧状态时要执行的清理动作
 */
static void radio_exit_state(radio_state_t old_state) {
    if (old_state != g_radio_ctx.current_state) {
        return;
    }
    RADIO_INFO("Exiting state: %s", radio_state_to_string(old_state));
    
    switch (old_state) {
        case RADIO_STATE_INIT:
            // 退出初始化状态时，停止从MCU获取内存的定时器
            radio_timer_stop(RADIO_TIMER_CHECK_HARDWARE_STATUS);
            // Radio服务初始化后，通常会等待系统或其他服务的激活指令，
            service_radio_update_payload_data();
            RADIO_DEBUG("g_radio_ctx.radio_reg_para->cur_freq = %d", g_radio_ctx.radio_reg_para->cur_freq);
            radio_state_payload_t* ready_payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            if (ready_payload) {

                // . 【核心】一次性发送多播消息
                bus_post_message(
                    SRC_RADIO_SERVICE,
                    ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, // 目的地是UI和System
                    MSG_PRIO_NORMAL,
                    RADIO_EVT_READY, // 事件类型
                    ready_payload,
                    SRC_RADIO_SERVICE,
                    NULL
                );
            }

            break;

        case RADIO_STATE_PLAYING:
            break;

        case RADIO_STATE_SEEKING:
            // 退出搜台状态时，必须停止更新UI的定时器
            radio_timer_stop(RADIO_TIMER_SEEK_UPDATE_UI);
            service_radio_update_payload_data();
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_FINISHED, payload, SRC_RADIO_SERVICE, NULL);

            break;

        case RADIO_STATE_TUNING:
        {
            radio_timer_stop(RADIO_TIMER_TUNE_STEP);
            radio_timer_stop(RADIO_TIMER_TUNE_STATUS);
             service_radio_update_payload_data();
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_FINISHED, payload, SRC_RADIO_SERVICE, NULL);

        }
        break;    

        case RADIO_STATE_AUTO_STORING:
        {
            radio_timer_stop(RADIO_TIMER_SEEK_UPDATE_UI);
            service_radio_update_payload_data();
            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_FINISHED, payload, SRC_RADIO_SERVICE, NULL);

        }
        break;
         case RADIO_STATE_SAVE_PRESET:
        {
            radio_timer_stop(RADIO_TIMER_PRESET_SAVE_STATUS);
        }
        break;
        case RADIO_STATE_PLAY_PRESET:
        {
            radio_timer_stop(RADIO_TIMER_PRESET_PLAY_STATUS);
        }
        break;

         case RADIO_STATE_SWITCH_BAND:
        {
            // 进入切换波段状态时，通知UI波段切换
            radio_timer_stop(RADIO_TIMER_SWITCH_BAND_STATUS);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SWITCH_BAND_FINISHED, NULL, SRC_UNDEFINED, NULL);
            break;
        }

        case RADIO_STATE_SWITCH_BAND_AND_TUNE:
            // 进入切换波段并微调状态时，通知UI波段切换
            radio_timer_stop(RADIO_TIMER_SWITCH_BAND_AND_TUNE_STATUS);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SWITCH_BAND_FINISHED, NULL, SRC_UNDEFINED, NULL);
            break;

        case RADIO_STATE_PTY_SEEK:
        {
            // 退出PTY搜台状态时，必须停止更新UI的定时器
            radio_timer_stop(RADIO_TIMER_SEEK_UPDATE_UI);
            service_radio_update_payload_data();
            radio_state_payload_t* pty_payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_UI, MSG_PRIO_NORMAL, RADIO_EVT_SEARCH_FINISHED, pty_payload, SRC_RADIO_SERVICE, NULL);
            break;    
        }

        case RADIO_STATE_RDS:
        {
            bus_post_message(SRC_RADIO_SERVICE, ADDRESS_SYSTEM_SERVICE, MSG_PRIO_NORMAL, SYSTEM_CMD_FORCE_RELEASE_DISPLAY, NULL, SRC_UNDEFINED, NULL);
        }
        break; 


        // ... 其他状态的退出清理动作 ...
        default:
            break;
    }
}

/**
 * @brief 将状态枚举转换为字符串，便于调试
 */
static const char* radio_state_to_string(radio_state_t state) {
    switch (state) {
        case RADIO_STATE_IDLE: return "IDLE";
        case RADIO_STATE_INIT: return "INIT";
        case RADIO_STATE_PLAYING: return "PLAYING";
        case RADIO_STATE_SEEKING: return "SEEKING";
        case RADIO_STATE_TUNING: return "TUNING";
        case RADIO_STATE_AUTO_STORING: return "AUTO_STORING";
        default: return "UNKNOWN";
    }
}

// --- START OF REFACTORED FUNCTION in service_radio.c ---

/**
 * @brief 【核心】状态机事件处理器 (重构版)
 * 
 * 严格遵循“先状态 -> 再来源 -> 再命令”的解析顺序。
 */
static void radio_event_handler(app_message_t* msg) {
    // 提前解开包装器，方便后续使用
    ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
    void* payload_ptr = wrapper ? wrapper->ptr : NULL;

    RADIO_DEBUG("State: [%s], Event: [0x%X] from [Source ID: 0x%X]", 
                radio_state_to_string(g_radio_ctx.current_state), msg->command, msg->source);

    // --- 1. 首先，处理任何状态下都必须响应的全局命令 ---
    // 这种命令不关心当前状态，例如 ping-pong 测试或强制关机指令。
    switch (msg->command) {
        case CMD_PING_REQUEST:
        case CMD_PING_BROADCAST:
            service_handle_ping_request(msg);
            return; // 优先处理并直接返回
        case CMD_PONG_RESPONSE:
            service_handle_pong_response(msg);
            return; // 优先处理并直接返回
    }

    // --- 2. 然后，根据当前状态对事件进行分发 ---
    switch (g_radio_ctx.current_state) {

        case RADIO_STATE_INIT: {
            switch (msg->source) {
                case SRC_TIMER_SERVICE: {
                    switch (msg->command) {
                        case RADIO_CMD_INTERNAL_TIMER_CHECK_HARDWARE_STATUS:
                            RADIO_DEBUG("FM init status: %d", radio_hal_fm_get_init_status());
                             if(radio_hal_fm_get_init_status() == 0)
                             {
                                radio_exit_state(RADIO_STATE_INIT);
                                radio_enter_state(RADIO_STATE_PLAYING);
                             }
                            break;
                            
                            default:
                            break;
                    }
                    break; // end of SRC_TIMER_SERVICE
                }
                case SRC_MCU_SERVICE: {
                    switch (msg->command) {
                            default:
                            break;
                    }
                    break; // end of SRC_MCU_SERVICE
                }
                default:
                    break;
            }
            break; // end of RADIO_STATE_INIT
        }


        // ====================================================================
        // == 状态: PLAYING (正常播放)
        // ====================================================================
        case RADIO_STATE_PLAYING: {
            switch (msg->source) {
                case SRC_UI: { // 处理来自UI的命令
                    switch (msg->command) {
                        case RADIO_CMD_PLAY_NEXT:
                            radio_seek_type_event(1); // 向下搜台
                            break;
                        case RADIO_CMD_PLAY_PREV:
                            radio_seek_type_event(0); // 向上搜台
                            break;
                        case RADIO_CMD_SEEK_NEXT:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_seek_down();
                            radio_enter_state(RADIO_STATE_SEEKING);
                            break;
                        
                        case RADIO_CMD_SEEK_PREV:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_seek_up();
                            radio_enter_state(RADIO_STATE_SEEKING);
                            break;

                        case RADIO_CMD_TUNE_UP:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_tune_up();
                            radio_enter_state(RADIO_STATE_TUNING);
                            break;
                        case RADIO_CMD_TUNE_DOWN:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_tune_down();
                            radio_enter_state(RADIO_STATE_TUNING);
                            break;
                        case RADIO_CMD_SWITCH_FM_BAND:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_switch_band(0);
                            radio_enter_state(RADIO_STATE_SWITCH_BAND);
                            break;
                        case RADIO_CMD_SWITCH_AM_BAND:   
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_switch_band(1);
                            radio_enter_state(RADIO_STATE_SWITCH_BAND);
                            break;
                        case RADIO_CMD_PLAY_PRESET:
                            {
                                radio_set_preset_payload_t *preset_payload = (radio_set_preset_payload_t *)payload_ptr;
                                if(preset_payload && preset_payload->preset_index >= 0 && preset_payload->preset_index < RADIO_MAX_CH)
                                {
                                    int preset_index = preset_payload->preset_index + preset_payload->band * RADIO_MAX_CH + 1;

                                    if(preset_payload->band != g_radio_ctx.radio_reg_para->cur_band)
                                    {
                                        RADIO_ERROR("Preset band %d does not match current band %d", preset_payload->band, g_radio_ctx.radio_reg_para->cur_band);
                                        radio_exit_state(RADIO_STATE_PLAYING);
                                        radio_hal_switch_band_and_tune(preset_payload->band, preset_payload->preset_index + 1);
                                        radio_enter_state(RADIO_STATE_SWITCH_BAND_AND_TUNE);
                                        break;
                                    }

                                    if(preset_index<=RADIO_MAX_PRESET && preset_index > 0)
                                    {
                                        RADIO_DEBUG("Playing preset index: %d", preset_index);
                                        radio_exit_state(RADIO_STATE_PLAYING);
                                        radio_hal_play_preset(preset_payload->preset_index+1);
                                        radio_enter_state(RADIO_STATE_PLAY_PRESET);
                                    }
                                    else
                                    {
                                        RADIO_ERROR("Invalid preset index: %d", preset_index);
                                    }
                                }
                                else
                                {
                                    RADIO_ERROR("Invalid preset payload received.");
                                }
                            }
                            break;

                        case RADIO_CMD_SAVE_PRESET:
                            {
                                radio_set_preset_payload_t *preset_payload = (radio_set_preset_payload_t *)payload_ptr;
					RADIO_INFO("preset_payload->preset_flag=%d\n",preset_payload->preset_flag);			
                                if(preset_payload && preset_payload->preset_flag)
                                {
                                    int preset_index = service_radio_find_null_slot_preset_list();

                                    if(preset_index == 0)
                                    {
                                        RADIO_ERROR("No available preset slot found.");
                                        break;
                                    }

                                    if(preset_index)
                                    {
                                        radio_exit_state(RADIO_STATE_PLAYING);
                                        radio_hal_set_preset(preset_index);
                                        radio_enter_state(RADIO_STATE_SAVE_PRESET);
                                    }
                                }
                                else if(preset_payload && preset_payload->preset_flag == 0)
                                {
                                    if(g_radio_ctx.radio_reg_para->cur_preset>=0 && g_radio_ctx.radio_reg_para->cur_preset < RADIO_MAX_PRESET)
                                    {
                                        int preset_index = 1;
                                        #ifdef NO_FM3_AM2_BAND
                                           preset_index= g_radio_ctx.radio_reg_para->cur_preset+1;
                                        #else
                                           preset_index = g_radio_ctx.radio_reg_para->cur_preset%6+1;
                                        #endif
                                        if(preset_index)
                                        {
                                            radio_exit_state(RADIO_STATE_PLAYING);
                                            radio_hal_cancel_preset(preset_index);
                                            radio_enter_state(RADIO_STATE_SAVE_PRESET);
                                        }
                                    }
                                    else {
                                        RADIO_ERROR("Current preset index is invalid: %d", g_radio_ctx.radio_reg_para->cur_preset);
                                        break;
                                    }
                                }
                            }
                        break;

                        case RADIO_CMD_SAVE_PRESET_FROM_LIST:
                            {
                                radio_set_preset_payload_t *preset_payload = (radio_set_preset_payload_t *)payload_ptr;
                                if(preset_payload && preset_payload->preset_flag)
                                {
                                    int preset_index = preset_payload->preset_index+preset_payload->band*RADIO_MAX_CH+1;

                                    if(preset_payload->band!= g_radio_ctx.radio_reg_para->cur_band)
                                    {
                                        RADIO_ERROR("Preset band %d does not match current band %d", preset_payload->band, g_radio_ctx.radio_reg_para->cur_band);
                                       break;
                                    }

                                    if(preset_index<=RADIO_MAX_PRESET)
                                    {
                                        radio_exit_state(RADIO_STATE_PLAYING);
                                        radio_hal_set_preset(preset_payload->preset_index+1);
                                        radio_enter_state(RADIO_STATE_SAVE_PRESET);
                                    }
                                }
                                else if(preset_payload && preset_payload->preset_flag == 0)
                                {
                                    int preset_index = preset_payload->preset_index+preset_payload->band*RADIO_MAX_CH+1;
                                    if(preset_index<=RADIO_MAX_PRESET)
                                    {
                                        radio_exit_state(RADIO_STATE_PLAYING);
                                        radio_hal_cancel_preset(preset_payload->preset_index+1);
                                        radio_enter_state(RADIO_STATE_SAVE_PRESET);
                                    }
                                }
                            }
                            break;
                        case RADIO_CMD_AUTO_STORE:
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_auto_store();
                            radio_enter_state(RADIO_STATE_AUTO_STORING);
                            break;
                        case RADIO_CMD_PTY_SEEK:
                        {
                            char pty_code = *((char *)payload_ptr);
                            if(pty_code < 0 || pty_code > 31) {
                                RADIO_ERROR("Invalid PTY code: %d", pty_code);
                                break; // 无效的PTY代码
                            }
                            RADIO_DEBUG("PTY seek requested with code: %d", pty_code);
                            radio_exit_state(RADIO_STATE_PLAYING);
                            radio_hal_pty_seek(pty_code);
                            radio_enter_state(RADIO_STATE_PTY_SEEK);
                        }
                            break;

                        case RADIO_CMD_SETUP:
                        {
                            radio_setup_payload_t* setup_payload = (radio_setup_payload_t*)payload_ptr;
                            if (setup_payload) {
                                if (radio_setup_event(setup_payload) == 0) {
                                    // 成功处理设置事件后，更新UI
                                    service_radio_update_payload_data();
                                    radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                                    if (payload) {
                                        bus_post_message(
                                            SRC_RADIO_SERVICE,
                                            ADDRESS_UI, // 目的地是UI和System
                                            MSG_PRIO_NORMAL,
                                            RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                            payload,
                                            SRC_RADIO_SERVICE,
                                            NULL
                                        );
                                    }
                                } else {
                                    RADIO_ERROR("Failed to handle setup event: type=%d, value=%d", setup_payload->setup_type, setup_payload->setting_value);
                                }
                            } else {
                                RADIO_ERROR("Setup payload is NULL.");
                            }
                        }
                        break;

                        case RADIO_CMD_TOGGLE_LOC_DX:
                            break;
                        case RADIO_CMD_TOGGLE_RDS:
                            break;
                        case RADIO_CMD_TOGGLE_RDS_AF:
                            break;
                        case RADIO_CMD_TOGGLE_RDS_TA:
                            break;
                        case RADIO_CMD_TOGGLE_RDS_CT:
                            break;
                        case RADIO_CMD_SWITCH_PTY_TYPE:
                            break;
                        case RADIO_CMD_SWITCH_AREA:
                            break;
      

                        // ... 在此添加其他播放状态下的UI命令处理 ...
                        // case RADIO_CMD_TUNE_UP: ...
                        // case RADIO_CMD_PLAY_PRESET: ...
                        
                        default:
                            RADIO_DEBUG("Unhandled command 0x%X from UI in PLAYING state.", msg->command);
                            break;
                    }
                    break; // end of SRC_UI
                }
                case SRC_TIMER_SERVICE: {
                    switch (msg->command) {
                        case RADIO_CMD_INTERNAL_TIMER_PLAYING_UPDATE_UI:
                        {
                            service_radio_update_payload_data();
                            g_radio_ctx.radio_data.cur_seeking_type = CUR_NO_SEEK;
                            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                            if (payload) {
                                // . 【核心】一次性发送多播消息
                                bus_post_message(
                                    SRC_RADIO_SERVICE,
                                    ADDRESS_UI, // 目的地是UI和System
                                    MSG_PRIO_NORMAL,
                                    RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                    payload,
                                    SRC_RADIO_SERVICE,
                                    NULL
                                );
                            }
                            radio_timer_stop(RADIO_TIMER_PLAYING_UPDATE_UI);
                        }
                        break;

                        case RADIO_CMD_INTERNAL_TIMER_CHECK_RDS_STATUS:    
                        {
                            // 检查RDS状态并更新UI
                            if(g_radio_ctx.radio_reg_para->cur_rds_status&RDS_IS_TRAFFIC || ((g_radio_ctx.radio_reg_para->cur_rds_status&(RDS_HAVE_TA)) && (g_radio_ctx.radio_reg_para->cur_rds_status&(RDS_HAVE_TP)) &&g_radio_ctx.init_reg_para->ta_on_off))
                            {
                                radio_exit_state(RADIO_STATE_PLAYING);
                                radio_enter_state(RADIO_STATE_RDS);
                            }
                        }
                        break;
                        
                    }
                    break; // end of SRC_TIMER_SERVICE
                }

                // 在此可以添加对其他来源（如 SRC_SYSTEM_SERVICE）消息的处理
                // case SRC_SYSTEM_SERVICE: { ... }

                default:
                    RADIO_DEBUG("Unhandled message source 0x%X in PLAYING state.", msg->source);
                    break;
            }
            break; // end of RADIO_STATE_PLAYING
        }


        // ====================================================================
        // == 状态: SEEKING (正在搜台)
        // ====================================================================
        case RADIO_STATE_SEEKING: {
            switch (msg->source) {
                case SRC_UI: { // 在搜台过程中，只响应少数几个命令
                    switch (msg->command) {
                        case RADIO_CMD_SEEK_NEXT:
                        case RADIO_CMD_SEEK_PREV:
                        case RADIO_CMD_TUNE_UP:
                        case RADIO_CMD_TUNE_DOWN:
                        case RADIO_CMD_STOP_SEEK:
                            RADIO_INFO("User interrupted seek operation.");
                            radio_exit_state(RADIO_STATE_SEEKING);
                            radio_hal_seek_stop();
                            radio_enter_state(RADIO_STATE_PLAYING);
                            break;

                        default:
                            RADIO_DEBUG("Command 0x%X from UI ignored during SEEKING.", msg->command);
                            break;
                    }
                    break; // end of SRC_UI
                }

                // 只有 Radio 服务自己(或定时器服务)才能发送内部命令
                case SRC_RADIO_SERVICE:
                case SRC_TIMER_SERVICE: {
                    switch (msg->command) {
                        case RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT:
                            {
                                service_radio_update_payload_data();
                                radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                                if (payload) {
                                    // . 【核心】一次性发送多播消息
                                    bus_post_message(
                                        SRC_RADIO_SERVICE,
                                        ADDRESS_UI, // 目的地是UI和System
                                        MSG_PRIO_NORMAL,
                                        RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                        payload,
                                        SRC_RADIO_SERVICE,
                                        NULL
                                    );
                                   // RADIO_DEBUG("cur time:%d-seeking update payload data to ui.",infotainment_get_ticks());
                                }

                                if(g_radio_ctx.radio_reg_para->search_stop_flag == 1)
                                {
                                    radio_exit_state(RADIO_STATE_SEEKING);
                                    radio_enter_state(RADIO_STATE_PLAYING);
                                }
                              
                            }
                            break;
                           
                    }
                    break; // end of SRC_RADIO_SERVICE / SRC_TIMER_SERVICE
                }

                default:
                    RADIO_DEBUG("Unhandled message source 0x%X in SEEKING state.", msg->source);
                    break;
            }
            break; // end of RADIO_STATE_SEEKING
        }
        case RADIO_STATE_TUNING:
        {
            switch (msg->source) {
                case SRC_UI: { // 在微调过程中，只响应少数几个命令
                    switch (msg->command) {
                        case RADIO_CMD_SEEK_PREV:
                        case RADIO_CMD_TUNE_UP:
                            radio_timer_start(RADIO_TIMER_TUNE_STATUS, 2000, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_TUNE_STATUS, NULL);
                            radio_hal_tune_up();
                            break;
                        case RADIO_CMD_SEEK_NEXT:
                        case RADIO_CMD_TUNE_DOWN:
                            radio_timer_start(RADIO_TIMER_TUNE_STATUS, 2000, TIMER_TYPE_ONE_SHOT, RADIO_CMD_INTERNAL_TIMER_TUNE_STATUS, NULL);
                            radio_hal_tune_down();
                            break;
                        case RADIO_CMD_STOP_SEEK:
                            break;
                        default:
                            RADIO_DEBUG("Command 0x%X from UI ignored during TUNING.", msg->command);
                            break;
                    }
                    break; // end of SRC_UI
                }
                case SRC_TIMER_SERVICE: {
                    switch (msg->command) {
                        case RADIO_CMD_INTERNAL_TIMER_TUNE_STEP:
                        {
                            // 退出微调状态时，必须停止步进定时器
                            // Radio服务初始化后，通常会等待系统或其他服务的激活指令，
                            service_radio_update_payload_data();
                            g_radio_ctx.radio_data.cur_seeking_type = CUR_MANUAL_SEEK;
                            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                            if (payload) {
                                // . 【核心】一次性发送多播消息
                                bus_post_message(
                                    SRC_RADIO_SERVICE,
                                    ADDRESS_UI, // 目的地是UI和System
                                    MSG_PRIO_NORMAL,
                                    RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                    payload,
                                    SRC_RADIO_SERVICE,
                                    NULL
                                );
                            }
                        }
                        break;
                        case RADIO_CMD_INTERNAL_TIMER_TUNE_STATUS:
                            radio_exit_state(RADIO_STATE_TUNING);
                            radio_enter_state(RADIO_STATE_PLAYING);
                        break;
                    }
                    break; // end of SRC_TIMER_SERVICE
                }
                default:
                    break;
            }
        }
        break;


        // ====================================================================
        // == 在此添加其他状态 (TUNING, AUTO_STORING 等) 的 case 块
        // ====================================================================
        
        case RADIO_STATE_AUTO_STORING: {
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令
                    switch (msg->command) {
                        case RADIO_CMD_SEEK_NEXT:
                        case RADIO_CMD_SEEK_PREV:
                        case RADIO_CMD_TUNE_UP:
                        case RADIO_CMD_TUNE_DOWN:
                        case RADIO_CMD_STOP_SEEK:
                            RADIO_INFO("User interrupted seek operation.");
                            radio_exit_state(RADIO_STATE_AUTO_STORING);
                            radio_hal_seek_stop();
                            radio_enter_state(RADIO_STATE_PLAYING);
                            break;

                        default:
                            RADIO_DEBUG("Command 0x%X from UI ignored during autoseeking.", msg->command);
                            break;
                    }
                    break; // end of SRC_UI
                }
                case SRC_TIMER_SERVICE: {
                    switch (msg->command) {
                        case RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT:
                        {
                            service_radio_update_payload_data();
                            radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                            if (payload) {
                                // . 【核心】一次性发送多播消息
                                bus_post_message(
                                    SRC_RADIO_SERVICE,
                                    ADDRESS_UI, // 目的地是UI和System
                                    MSG_PRIO_NORMAL,
                                    RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                    payload,
                                    SRC_RADIO_SERVICE,
                                    NULL
                                );
                                // RADIO_DEBUG("cur time:%d-seeking update payload data to ui.",infotainment_get_ticks());
                            }
                            if(g_radio_ctx.radio_reg_para->search_stop_flag == 1)
                            {
                                radio_exit_state(RADIO_STATE_AUTO_STORING);
                                radio_enter_state(RADIO_STATE_PLAYING);
                            }
                        }
                        break;
                    }
                    break; // end of SRC_TIMER_SERVICE
                }
                break; // end of SRC_TIMER_SERVICE

                default:
                    break;
            }
        }
        break;

        case RADIO_STATE_SAVE_PRESET: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令

                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_PRESET_SAVE_STATUS)
                    {
                        radio_exit_state(RADIO_STATE_SAVE_PRESET);
                        radio_enter_state(RADIO_STATE_PLAYING);
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }


         case RADIO_STATE_PLAY_PRESET: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令

                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_PRESET_PLAY_STATUS)
                    {
                        radio_exit_state(RADIO_STATE_PLAY_PRESET);
                        radio_enter_state(RADIO_STATE_PLAYING);
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }

        case RADIO_STATE_SWITCH_BAND: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令

                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_SWITCH_BAND_STATUS)
                    {
                        radio_exit_state(RADIO_STATE_PLAY_PRESET);
                        radio_enter_state(RADIO_STATE_PLAYING);
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }

        case RADIO_STATE_SWITCH_BAND_AND_TUNE: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令
                    
                    break; // end of SRC_UI
                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT)
                    {
                        radio_exit_state(RADIO_STATE_SWITCH_BAND_AND_TUNE);
                        radio_enter_state(RADIO_STATE_PLAYING);
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }

        case RADIO_STATE_PTY_SEEK: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令
                    switch (msg->command) {
                        case RADIO_CMD_SEEK_NEXT:
                        case RADIO_CMD_SEEK_PREV:
                        case RADIO_CMD_TUNE_UP:
                        case RADIO_CMD_TUNE_DOWN:
                        case RADIO_CMD_STOP_SEEK:
                            RADIO_INFO("User interrupted seek operation.");
                            radio_exit_state(RADIO_STATE_PTY_SEEK);
                            radio_hal_seek_stop();
                            radio_enter_state(RADIO_STATE_PLAYING);
                            break;

                        default:
                            RADIO_DEBUG("Command 0x%X from UI ignored during autoseeking.", msg->command);
                            break;
                    }
                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_SEEK_TIMEOUT)
                    {
                        service_radio_update_payload_data();
                        radio_state_payload_t* payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
                        if (payload) {
                            // . 【核心】一次性发送多播消息
                            bus_post_message(
                                SRC_RADIO_SERVICE,
                                ADDRESS_UI, // 目的地是UI和System
                                MSG_PRIO_NORMAL,
                                RADIO_EVT_UPDATE_FREQUENCY, // 事件类型
                                payload,
                                SRC_RADIO_SERVICE,
                                NULL
                            );
                            // RADIO_DEBUG("cur time:%d-seeking update payload data to ui.",infotainment_get_ticks());
                        }
                        if(g_radio_ctx.radio_reg_para->search_stop_flag == 1)
                        {
                            radio_exit_state(RADIO_STATE_PTY_SEEK);
                            radio_enter_state(RADIO_STATE_PLAYING);
                        }
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }

        case RADIO_STATE_RDS: {
            // 在空闲状态，可能只响应“开机”或模式切换的命令
            switch (msg->source) {
                case SRC_UI: { // 在自动存储过程中，只响应少数几个命令
                    
                    break; // end of SRC_UI
                }
                break;
                case SRC_TIMER_SERVICE: { // 在自动存储过程中，只响应少数几个命令
                    if(msg->command == RADIO_CMD_INTERNAL_TIMER_CHECK_RDS_STATUS)
                    {

                        if(g_radio_ctx.radio_reg_para->cur_rds_status&RDS_IS_TRAFFIC || ((g_radio_ctx.radio_reg_para->cur_rds_status&(RDS_HAVE_TA)) && (g_radio_ctx.radio_reg_para->cur_rds_status&(RDS_HAVE_TP)) &&g_radio_ctx.init_reg_para->ta_on_off))
                        {
                            //rds play
                        }
                        else
                        {
                            radio_exit_state(RADIO_STATE_RDS);
                            radio_enter_state(RADIO_STATE_PLAYING);
                        }
                    }
                }
                break;

                default:
                break;
            }
            break; // end of RADIO_STATE_IDLE
        }



        // ====================================================================
        // == 状态: IDLE (空闲)
        // ====================================================================
        case RADIO_STATE_IDLE: {
            switch (msg->command) {
                // case RADIO_CMD_TURN_ON:
                //     radio_enter_state(RADIO_STATE_PLAYING);
                //     break;
            }
            break; // end of RADIO_STATE_IDLE
        }
        
        default:
            RADIO_ERROR("Unhandled state: %d", g_radio_ctx.current_state);
            break;
    }
}

//获取服务注册表共享数据
static void radio_get_service_reg_data(void)
{
    g_radio_ctx.radio_reg_para = dsk_reg_get_para_by_app(REG_APP_FM);
    g_radio_ctx.init_reg_para = dsk_reg_get_para_by_app(REG_APP_INIT);
}




// --- END OF REFACTORED FUNCTION ---
/**
 * @brief Radio服务线程的入口函数
 */
static void radio_thread_entry(void* p_arg) {
    unsigned char err = 0;
    RADIO_INFO("Thread created with handle %d", service_radio_thread_handle);


    // 1. 创建消息队列
    service_radio_queue_handle = infotainment_queue_create(RADIO_QUEUE_SIZE);
    if (!service_radio_queue_handle) {
        RADIO_ERROR("Failed to create message queue");
        infotainment_thread_delete(service_radio_thread_handle);
        return;
    }
    RADIO_INFO("Message queue created with handle %p", service_radio_queue_handle);

    // 2. 注册服务到消息总线路由器
    if (bus_router_register_service(SRC_RADIO_SERVICE, ADDRESS_RADIO_SERVICE,service_radio_queue_handle,"Radio service",service_radio_payload_free,service_radio_payload_copy) < 0) {
        RADIO_ERROR("Failed to register service with bus router");
        infotainment_queue_delete(service_radio_queue_handle, &err);
        infotainment_thread_delete(service_radio_thread_handle);
        return;
    }

    // 5. 进入初始状态
    if(radio_hal_fm_get_init_status() == 0)
    { 
        service_radio_update_payload_data();
            RADIO_DEBUG("g_radio_ctx.radio_reg_para->cur_freq = %d", g_radio_ctx.radio_reg_para->cur_freq);
        radio_state_payload_t* ready_payload = service_radio_payload_copy(&g_radio_ctx.radio_data);
        if (ready_payload) {

            // . 【核心】一次性发送多播消息
            bus_post_message(
                SRC_RADIO_SERVICE,
                ADDRESS_UI | ADDRESS_SYSTEM_SERVICE, // 目的地是UI和System
                MSG_PRIO_NORMAL,
                RADIO_EVT_READY, // 事件类型
                ready_payload,
                SRC_RADIO_SERVICE,
                NULL
            );
        }
        radio_enter_state(RADIO_STATE_PLAYING);
    }
    else
    {
        radio_enter_state(RADIO_STATE_INIT);
    }

    radio_timer_start(RADIO_TIMER_CHECK_RDS_STATUS, 500, TIMER_TYPE_PERIODIC, RADIO_CMD_INTERNAL_TIMER_CHECK_RDS_STATUS, NULL);


    while (1) {

        if (infotainment_thread_TDelReq(EXEC_prioself))
        {
            goto exit;
        }

        // 3. 等待并接收消息
        app_message_t* received_msg = (app_message_t*)infotainment_queue_pend(service_radio_queue_handle, 0, &err);
        if (!received_msg || err != OS_NO_ERR) {
            // 在实际系统中，pend可能会因为超时返回NULL，这里可以添加超时处理
            continue; // 没有消息，继续等待
        }

        // 【核心】将所有消息都交给事件处理器
        radio_event_handler(received_msg);
        
        // 5. 释放消息及其载荷的内存
        if (received_msg->payload) {
            // 注意：因为我们使用了自定义的分配器，所以也应该使用自定义的释放器
            free_ref_counted_payload((ref_counted_payload_t *)received_msg->payload);
        }
        infotainment_free(received_msg);
        received_msg = NULL;
    }

exit:
    infotainment_thread_delete(EXEC_prioself);// 6. 清理工作
}

/**
 * @brief 初始化Radio服务
 * 负责创建并启动后台线程。
 */
void service_radio_init(void) {
    unsigned char err = 0;
    RADIO_INFO("Initializing radio service...");

     // 1. 初始化服务上下文
    memset(&g_radio_ctx, 0, sizeof(g_radio_ctx));
    // 设置初始电台数据 (可以从持久化存储中读取)
    g_radio_ctx.radio_data.frequency = 87500;
    strncpy(g_radio_ctx.radio_data.band_str, "FM1", sizeof(g_radio_ctx.radio_data.band_str) - 1);
    

    service_radio_semaphore = infotainment_sem_create(1);
    if (!service_radio_semaphore) {
        RADIO_ERROR("Failed to create semaphore");
        return;
    }
    radio_hal_init();
    radio_get_service_reg_data();

    // 创建并启动线程
    service_radio_thread_handle = infotainment_thread_create(
        radio_thread_entry, 
        NULL, 
        TASK_PROI_LEVEL3, // 假设宏已定义
        0x8000, 
        "ServiceRadioThread"
    );
    if(service_radio_thread_handle < 0) {
        RADIO_ERROR("Failed to create thread");
        infotainment_sem_delete(service_radio_semaphore, &err);
        service_radio_semaphore = NULL;
        return;
    }
}

/**
 * @brief 退出并清理Radio服务
 * 负责停止线程和释放相关资源。
 */
void service_radio_exit(void) {
    unsigned char err = 0;
    RADIO_INFO("Exiting radio service...");

    // 1. 注销服务
    bus_router_unregister_service(SRC_RADIO_SERVICE);

    // 2. 删除线程
    // 在实际应用中，需要一种安全的机制来通知线程退出while(1)循环
    if (service_radio_thread_handle > 0) {
        if(service_radio_queue_handle)
            infotainment_queue_post(service_radio_queue_handle, NULL);
        while(infotainment_thread_delete_req(service_radio_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            if(service_radio_queue_handle)
                infotainment_queue_post(service_radio_queue_handle, NULL);
            infotainment_thread_sleep(10);
        }
        service_radio_thread_handle = 0;
    }

    // 3. 删除消息队列
    // 删除队列前，应确保所有等待的线程都已退出
    if (service_radio_queue_handle) {
        unsigned char err = 0;
        void* msg = NULL;
        if (service_radio_semaphore)
            infotainment_sem_wait(service_radio_semaphore, 0, &err);
        while ((msg = infotainment_queue_get(service_radio_queue_handle, &err)) && err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg && app_msg->payload) {
                free_ref_counted_payload((ref_counted_payload_t*)app_msg->payload);
            }
            if(app_msg)
                infotainment_free(app_msg);
        }
        infotainment_queue_delete(service_radio_queue_handle, &err);
        service_radio_queue_handle = NULL;
        if (service_radio_semaphore)
            infotainment_sem_post(service_radio_semaphore);
    }
    if (service_radio_semaphore) {
        infotainment_sem_delete(service_radio_semaphore, &err);
        service_radio_semaphore = NULL;
    }
    radio_hal_deinit();
    RADIO_INFO("Radio service cleanup complete");
}


