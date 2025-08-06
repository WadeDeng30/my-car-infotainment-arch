#include <ui_manger.h>
#include "ui_screens.h"

#include <elibs_stdio.h>
#include <elibs_string.h>
#include <stdlib.h>
#include <lv_drv_disp.h>
#include <lv_drv_indev.h>
#include <kapi.h>

// --- 调试功能相关定义 ---
static int g_ui_debug = 1;      // 调试开关，默认关闭
static int g_ui_info = 0;       // 信息开关，默认关闭

// 调试信息打印宏 - 添加行号显示
#define UI_DEBUG(fmt, ...) \
    do { \
        if (g_ui_debug) { \
            eLIBs_printf("[UI-DEBUG:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define UI_INFO(fmt, ...) \
    do { \
        if (g_ui_info) { \
            eLIBs_printf("[UI-INFO:%d] " fmt "\n", __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define UI_ERROR(fmt, ...) \
    eLIBs_printf("[UI-ERROR:%d] " fmt "\n", __LINE__, ##__VA_ARGS__)


static void *g_ui_queue_handle = NULL; // UI线程的消息队列句柄
static int g_ui_thread_handle = 0; // UI线程的句柄
static bool g_ui_exiting = false; // UI线程退出标志
static void* g_ui_semaphore = NULL; // UI线程的信号量，用于同步

fast_lane_msg_t temp_buffer[FAST_LANE_BUFFER_SIZE];


//extern ui_screen_id_t g_manger_screen;




#define UI_QUEUE_SIZE 128

// --- 外部控制调试开关的函数 ---
void ui_set_debug(int enable) {
    g_ui_debug = enable;
    UI_INFO("Debug messages %s", enable ? "ENABLED" : "DISABLED");
}

void ui_set_info(int enable) {
    g_ui_info = enable;
    UI_INFO("Info messages %s", enable ? "ENABLED" : "DISABLED");
}

#define LVGL_FONT_SIZE_S			16
#define LVGL_FONT_SIZE_M			22
#define LVGL_FONT_SIZE_L			30
#define LVGL_FONT_SIZE_XL			36
#define LVGL_FONT_SIZE_XXL			55

lv_ft_info_t lv_font_ssmall;//16	//16
lv_ft_info_t lv_font_small;//20		//21
lv_ft_info_t lv_font_medium;//24	//29
lv_ft_info_t lv_font_medium_ex;//24	//29

lv_ft_info_t lv_font_large;//40		//36
lv_ft_info_t lv_font_xlarge;

void freetype_font_ttf(void)
{
	/*FreeType uses C standard file system, so no driver letter is required.*/
	lv_font_ssmall.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_ssmall.weight = LVGL_FONT_SIZE_S;
	lv_font_ssmall.style = FT_FONT_STYLE_NORMAL;
	lv_font_ssmall.mem = NULL;//font_pbuf;
	//music_font_ssmall.mem_size = file_len;

	if(!lv_ft_font_init(&lv_font_ssmall)) {
		__err("music_font_ssmall font create failed.\n");
	}
	
	lv_font_small.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_small.weight = LVGL_FONT_SIZE_M;
	lv_font_small.style = FT_FONT_STYLE_NORMAL;
	lv_font_small.mem = NULL;//font_pbuf;
	//music_font_ssmall.mem_size = file_len;

	if(!lv_ft_font_init(&lv_font_small)) {
		__err("music_font_ssmall font create failed.\n");
	}

	lv_font_medium.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_medium.weight = LVGL_FONT_SIZE_L;
	lv_font_medium.style = FT_FONT_STYLE_NORMAL;
	lv_font_medium.mem = NULL;//font_pbuf;
	//music_font_small.mem_size = file_len;
		

	if(!lv_ft_font_init(&lv_font_medium)) {
		__err("music_font_small font create failed.\n");
	}

	lv_font_medium_ex.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_medium_ex.weight = LVGL_FONT_SIZE_L;
	lv_font_medium_ex.style = FT_FONT_STYLE_BOLD;
	lv_font_medium_ex.mem = NULL;//font_pbuf;
	//music_font_small.mem_size = file_len;
		

	if(!lv_ft_font_init(&lv_font_medium_ex)) {
		__err("music_font_small font create failed.\n");
	}	

	lv_font_large.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_large.weight = LVGL_FONT_SIZE_XL;
	lv_font_large.style = FT_FONT_STYLE_NORMAL;
	lv_font_large.mem = NULL;//font_pbuf;
	//music_font_small.mem_size = file_len;

	if(!lv_ft_font_init(&lv_font_large)) {
		__err("music_font_small font create failed.\n");
	}

	lv_font_xlarge.name = "d:\\res\\fonts\\font.ttf";//name;//"./lvgl/examples/libs/freetype/arial.ttf";
	lv_font_xlarge.weight = LVGL_FONT_SIZE_XXL;
	lv_font_xlarge.style = FT_FONT_STYLE_NORMAL;
	lv_font_xlarge.mem = NULL;//font_pbuf;
	//music_font_small.mem_size = file_len;

	if(!lv_ft_font_init(&lv_font_xlarge)) {
		__err("music_font_small font create failed.\n");
	}
}



// 比较函数，用于qsort排序
int compare_fast_lane_msg(const void* a, const void* b) {
    const fast_lane_msg_t* msg_a = (const fast_lane_msg_t*)a;
    const fast_lane_msg_t* msg_b = (const fast_lane_msg_t*)b;
    // 优先级高的排在前面
    return msg_b->priority - msg_a->priority;
}



// 修改UI线程消息处理部分
static void ui_thread_entry(void* p_arg) {
    unsigned char err = 0;

    // 等待2秒后启动ping-pong测试
   // printf("UI Thread: 等待2秒后开始Ping-Pong测试...\n");
   // infotainment_thread_sleep(200);  // 2000ms = 200 ticks (200 * 10ms = 2000ms)
  //  ui_start_ping_pong_test();
    
    // 进入主循环
      //ui_screen_create_initial(g_manger_screen); 
    while (1) {
        if (infotainment_thread_TDelReq(EXEC_prioself)) {
            g_ui_exiting = true;
            goto exit;
        }

        app_message_t* msg = (app_message_t*)infotainment_queue_get(g_ui_queue_handle, &err);
        if (msg && err == OS_NO_ERR) {
            UI_DEBUG("收到消息 (来源:0x%x, 命令:0x%x)", msg->source, msg->command);
            
            // 检查是否是ping-pong消息
            if (msg->command == CMD_PING_REQUEST || msg->command == CMD_PING_BROADCAST) {
                service_handle_ping_request(msg);
            }
            else if (msg->command == CMD_PONG_RESPONSE) {
                service_handle_pong_response(msg);
            }
            else {
                ui_manager_dispatch_message(msg);
            }
            //__msg("msg=%x\n",msg);	
		// __msg("msg->payload=%x\n",msg->payload);	
            if (msg->payload) {
               // UI_DEBUG("1=0x%2x\n", (int)msg->payload);
                free_ref_counted_payload((ref_counted_payload_t*)msg->payload);
               // UI_DEBUG("2=0x%2x\n", (int)msg);
                msg->payload = NULL;
            }
		//__msg("msg=%x\n",msg);	
            infotainment_free(msg);
            msg = NULL;
            lv_timer_handler();
			//__msg("lv_timer_handler\n");
            continue;
        }

        // 处理快速通道消息...
       // eLIBs_printf("UI Thread: 等待快速通道消息...\n");
        infotainment_sem_wait(g_router_fast_lane_semaphore, 1, &err);
       // eLIBs_printf("UI Thread: 快速通道消息处理完成 %d\n",err);

        if (err == OS_TIMEOUT) {
            uint32_t msg_count = bus_router_fast_lane_read_all(temp_buffer, FAST_LANE_BUFFER_SIZE);
            if (msg_count > 0) {
                qsort(temp_buffer, msg_count, sizeof(fast_lane_msg_t), compare_fast_lane_msg);
                for (uint32_t i = 0; i < msg_count; ++i) {
                    // handle_fast_lane_message(&temp_buffer[i]);
                }
            }
            infotainment_sem_post(g_router_fast_lane_semaphore);
        }
        //__msg("lv_timer_handler\n");	
        lv_timer_handler();
	// __msg("lv_timer_handler11\n");	
        //infotainment_thread_sleep(1);  // 10ms = 1 tick (避免100%CPU占用)
    }

    exit:

        infotainment_thread_delete(EXEC_prioself);
}



void ui_main_init(void) {
    unsigned char err = 0;
    UI_INFO("Initializing UI thread...");

      // 1. 初始化：创建自己的队列，并向路由器注册
     g_ui_queue_handle = infotainment_queue_create(UI_QUEUE_SIZE); // 创建UI线程的消息队列
    if (!g_ui_queue_handle) {
        UI_ERROR("Failed to create message queue");
        goto exit; // 队列创建失败，退出线程
    }
    bus_router_register_service(SRC_UI,ADDRESS_UI, g_ui_queue_handle,"UI Service",NULL,NULL);
    UI_INFO("Registered with router");
    memset(temp_buffer, 0, sizeof(temp_buffer)); // 清空临时缓冲区
    // 2. 创建信号量
    g_ui_semaphore = infotainment_sem_create(1);
    if (!g_ui_semaphore) {
        UI_ERROR("Failed to create semaphore");
        goto exit; // 信号量创建失败，退出线程
    }

    // 2. 初始化LVGL、UI管理器和输入驱动
     lv_init();
     lv_port_disp_init();
     lv_port_indev_init();
     freetype_font_ttf();
     //input_driver_init(); // 【新增】调用输入驱动初始化
     ui_screens_init();
     ui_manager_init();
    UI_INFO("LVGL, Input Driver, and UI Manager initialized");



    // 创建并启动UI线程
    g_ui_thread_handle = infotainment_thread_create(ui_thread_entry,NULL,TASK_PROI_LEVEL3,0x8000,"UI task");
    if(g_ui_thread_handle == 0)
    {
        UI_ERROR("infotainment_thread_create fail");
    }
    UI_INFO("UI Thread created successfully with handle %d", g_ui_thread_handle);
    return; // 成功返回

    exit:
    bus_router_unregister_service(SRC_UI); // 注销UI服务
    // 释放UI线程资源
    if(g_ui_thread_handle > 0) {
        while(infotainment_thread_delete_req(g_ui_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            infotainment_thread_sleep(10);
        } // 请求删除UI线程
        g_ui_thread_handle = 0;
    }
    if (g_ui_queue_handle) {
        unsigned char err = 0;
        void* msg = NULL;
        // 不断从队列取出消息并释放，直到队列为空
        while ((msg = infotainment_queue_get(g_ui_queue_handle, &err)) && err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg&&app_msg->payload) {

                app_msg->payload = NULL; // 清空指针
            }
            if(app_msg)
                infotainment_free(app_msg); // 释放消息结构体内存
            app_msg = NULL;
        }
        infotainment_queue_delete(g_ui_queue_handle, &err); // 删除消息队列
        g_ui_queue_handle = NULL;
    }
    if (g_ui_semaphore) {
        unsigned char err = 0;
        infotainment_sem_delete(g_ui_semaphore, &err); // 删除信号量
        g_ui_semaphore = NULL;
    }
}

void ui_main_exit(void)
{
    unsigned char err = 0;

    bus_router_unregister_service(SRC_UI); // 注销UI服务
    // 释放UI线程资源
   if(g_ui_thread_handle > 0) {
        while(infotainment_thread_delete_req(g_ui_thread_handle)!=OS_TASK_NOT_EXIST)
        {
            infotainment_thread_sleep(10);
        } // 请求删除UI线程
        g_ui_thread_handle = 0;
    }
    if (g_ui_queue_handle) {
        unsigned char err = 0;
        void* msg = NULL;
        // 不断从队列取出消息并释放，直到队列为空
        while ((msg = infotainment_queue_get(g_ui_queue_handle, &err)) && err == OS_NO_ERR) {
            app_message_t* app_msg = (app_message_t*)msg;
            if (app_msg->payload) {
                free_ref_counted_payload((ref_counted_payload_t*)app_msg->payload); // 释放消息载荷内存
                app_msg->payload = NULL; // 清空指针
            }
            if(app_msg)
                infotainment_free(app_msg); // 释放消息结构体内存
            app_msg = NULL;
        }
        infotainment_queue_delete(g_ui_queue_handle, &err); // 删除消息队列
        g_ui_queue_handle = NULL;
    }
    if (g_ui_semaphore) {
        unsigned char err = 0;
        infotainment_sem_delete(g_ui_semaphore, &err); // 删除信号量
        g_ui_semaphore = NULL;
    }
    g_ui_exiting = true; // 设置退出标志
    UI_INFO("UI Thread exiting...");
}
