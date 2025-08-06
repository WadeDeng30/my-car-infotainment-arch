#include "infotainment_main.h"
#include "bus_router.h"
#include <ui_manger.h>
#include "bus_router_test.h" // 【修改】包含新的测试头文件
#include "service_msg.h"     // 【新增】包含消息服务头文件
#include "service_mcu.h"     // 【新增】包含MCU服务头文件



#define LANG_DIR            BEETLES_APP_ROOT"apps\\lang.bin"
#define THEME_DIR           BEETLES_APP_ROOT"apps\\theme.bin"


uint32_t custom_tick_get(void) {
    return esKRNL_TimeGet() * 10;
}


int32_t live_init(void)
{
	__msg("dsk_langres_init\n");
	dsk_langres_init(LANG_DIR);
	__msg("dsk_theme_init\n");
	dsk_theme_init(THEME_DIR);
	__msg("dsk_theme_init\n");
	{
		reg_system_para_t	*para = NULL;

		__inf("~~~~~~~~~~~~~~before dsk_reg_get_para_by_app~~~~~~~~~~~");
		para	= (reg_system_para_t *)dsk_reg_get_para_by_app(REG_APP_SYSTEM);

		if (para != NULL)
		{
			dsk_send_mcu_beep_on_off(para->keytone);
			dsk_volume_set(AUDIO_DEVICE_VOLUME_MAX);
#ifdef BP1064_MODEL
			dsk_set_audio_if(AUDIO_DEV_IF_CODEC);
#else
			dsk_set_audio_if(AUDIO_DEV_IF_IIS);
#endif			
			
			__msg("para->volume=%d\n", para->volume); 
			
			dsk_langres_set_type(para->language);
			__msg("para->language=%d", para->language);
			para->output = LION_DISP_LCD;	
		}
		else
		{
			dsk_display_set_lcd_bright(LION_BRIGHT_LEVEL12);		
			dsk_volume_set(AUDIO_DEVICE_VOLUME_MAX);
#ifdef BP1064_MODEL
			dsk_set_audio_if(AUDIO_DEV_IF_CODEC);
#else
			dsk_set_audio_if(AUDIO_DEV_IF_IIS);
#endif			
			dsk_langres_set_type(EPDK_LANGUAGE_ENM_CHINESES);
		}
		
	}
}


void shell_init_fm_dev(void)
{
	__s32 ret=0;
	int fm_handle=NULL;


	fm_handle = open("/dev/fm",O_RDWR);
	
	if(fm_handle < 0)
	{
		__log(" Error: open fm driver failed!\n ");
		return ;
	}

	__msg("DRV_FM_CMD_INIT\n");
	ret = ioctl(fm_handle, DRV_FM_CMD_INIT, 0);

	close(fm_handle);
	
	
}


static void shell_try_load_fm_drv(void)
{
	__u32					arg[3];
	int p_disp;
	reg_init_para_t  *init_para=NULL;

	reg_all_para_t* all_app_para = NULL;
	int		   fmcu;				//twi bus handle
	__hdle paramter_hdle=NULL;

	__msg("shell_try_load_fm_drv\n");
	fmcu = open("/dev/mcu",O_RDWR);

	if(fmcu >= 0)
	{
		paramter_hdle=ioctl(fmcu, MCU_GET_PARAMTER_HDLE, (void *)0);

		if(paramter_hdle >= 0)
		{
			all_app_para = (reg_all_para_t *)paramter_hdle;
			__msg("all_app_para=0x%2x\n",all_app_para);
			
			if(all_app_para)
			{
				init_para = &all_app_para->para_current.init_para;
			}

			
			
		}

		close(fmcu);
		fmcu=NULL;
	}


	#ifdef BOOT_SPEED
	__msg("init_para->plugin_fm_drv=%d\n", init_para->plugin_fm_drv);
	if(init_para->plugin_fm_drv)
		return ;
	#endif


#ifdef TUNER_SAVE_RDS_AF_LIST
		if(init_para->init_zone1 && init_para->init_zone2 &&init_para->init_zone3 &&init_para->init_zone4&& init_para->init_zone5&& init_para->init_zone6&& init_para->init_zone7)//ȷ����������
#else
		if(init_para->init_zone1 && init_para->init_zone2 &&init_para->init_zone3 &&init_para->init_zone4&& init_para->init_zone5)//ȷ����������
#endif	
		{
			//esKRNL_TimeDly(50);
			esDEV_Plugin("\\drv\\fm.drv", 0, 0, 1);
			shell_init_fm_dev();
			init_para->plugin_fm_drv = 1;
		}
#ifdef TUNER_SAVE_RDS_AF_LIST
		else
		{
			__msg("wait mcu data time out\n");
			esKRNL_TimeDly(200);
			
			if(init_para->init_zone1 && init_para->init_zone2 &&init_para->init_zone3 &&init_para->init_zone4&& init_para->init_zone5&& init_para->init_zone6&& init_para->init_zone7)//ȷ����������
			{
				esDEV_Plugin("\\drv\\fm.drv", 0, 0, 1);
				if(init_para->plugin_fm_drv)
					return ;
				shell_init_fm_dev();
				init_para->plugin_fm_drv = 1;
			}
	
		}
#endif


	
}
#ifdef BT_LINEIN
void shell_init_audio_drv(void)
{

	int bt_dev;
	bt_dev = open("/dev/bp1064",O_RDWR);
	if(bt_dev < 0)
		;//__home_msg(" Error: open fm driver failed! ");
	else
	{
		ioctl(bt_dev, BP1064_MOUDEL_CMD_AUDIO_START_INIT, (void *)NULL);		
		close(bt_dev);
	}
	
}
#endif


static void shell_try_load_tp_drv(void)
{
	__u32					arg[3];
	int p_disp;
	reg_init_para_t  *init_para=NULL;

	reg_all_para_t* all_app_para = NULL;
	int		   fmcu;				//twi bus handle
	__hdle paramter_hdle=NULL;

		
	fmcu = open("/dev/mcu",O_RDWR);

	if(fmcu >= 0)
	{
		paramter_hdle=ioctl(fmcu, MCU_GET_PARAMTER_HDLE, (void *)0);

		if(paramter_hdle >= 0)
		{
			all_app_para = (reg_all_para_t *)paramter_hdle;
			__msg("all_app_para=0x%2x\n",all_app_para);
			
			if(all_app_para)
			{
				init_para = &all_app_para->para_current.init_para;
			}

			
			
		}

		close(fmcu);
		fmcu=NULL;
	}


	if(init_para->tuner_mode_zone)//ȷ����������
	{
		__log("init_para->tuner_mode_zone\n");
		esDEV_Plugin("\\drv\\touchpanel.drv", 0, 0, 1); 	
		init_para->plugin_touch_drv = 1;
	}
	

	
}


__s32 res_init(void *p_arg)
{
    int32_t             ret = EPDK_FAIL;
    __msg("******start to dsk_reg_init_para******");
    ret = dsk_reg_init_para();
    __msg("******finish dsk_reg_init_para******");

    if (EPDK_FAIL == ret)
    {
        __msg("dsk_reg_init_para fail...");
    }

	msg_emit_init();
	live_init();
	rat_init();
	shell_try_load_tp_drv();
	shell_try_load_fm_drv();
#ifdef BT_LINEIN
	shell_init_audio_drv();
#endif
	{
		__s32 ret = 0;
		uint8_t code1[32] ={92,52,34,48,47,63,47,3,92,86,51,1,8,60,5,16,33,65,36,44,53,58,90,79,15,78,32,53,58,34,46,61};
		uint8_t code2[32] ={92,69,73,68,63,55,84,61,16,62,35,92,28,48,50,43,72,5,4,51,1,35,86,18,20,81,33,13,90,83,7,73}; 
		 

		ret = app_license_check_init(p_arg, code1, code2);
		printf("app_license_check_init :%d..\n",ret);
		
	}
	return EPDK_OK;
}



/**
 * @brief 系统主入口函数
 *
 * 负责初始化所有核心模块和服务
 */
int infotainment_main(void *p_arg)
{
    printf("\n\n=============== Infotainment System Starting ===============\n");
 	res_init(p_arg);

    // --- 1. 核心底层和驱动初始化 ---
    sunxifb_mem_init(); // Framebuffer 内存初始化
    // ... 其他底层硬件初始化 ...

    // --- 2. 核心服务和UI框架初始化 (注意顺序) ---
    
    // a. 消息总线必须第一个初始化
    bus_router_init();

    // b. UI线程和UI服务紧随其后，以便能接收其他服务的消息
    ui_main_init();

    // c. 初始化定时器服务和其他业务服务
     printf("\n\n===============service_timer_init ===============\n");
    service_timer_init();
	printf("init service_mcu_init\n");
    service_system_init();
	printf("init service_timer_init\n");
    service_msg_init();      // 【新增】初始化消息转换服务
	printf("init service_msg_init\n");
    service_mcu_init();      // 【新增】初始化MCU管理服务
	printf("init service_system_init\n");
    service_radio_init();
	printf("init service_radio_init\n");


    // ... 在此添加其他服务的 init 调用, e.g., service_bt_init() ...

    // --- 3. LVGL 显示和输入设备初始化 ---
   // lv_port_disp_init();
	printf("init lv_port_disp_init\n");

   // lv_port_indev_init();
	printf("init lv_port_indev_init\n");

    
	printf("init res_init\n");


    // --- 5. 设置调试日志级别 (可选) ---
    // 在这里可以根据系统配置，统一控制所有模块的日志输出
	#if 0
    bus_router_set_debug(1);
    bus_router_set_info(1);
    bus_router_set_ping_pong_log(1);

    service_system_set_debug(1);
    service_system_set_info(1);

    service_radio_set_debug(1);
    service_radio_set_info(1);
    
    service_timer_set_debug(1);
    service_timer_set_info(1);
    
    ui_set_debug(1);
    ui_set_info(1);
	#endif

	#if 0//def ENABLE_BUS_TESTING
    // === 开始自动化测试 ===
    printf("\n\n=============== Starting Automated Bus Router Tests ===============\n");
    infotainment_thread_sleep(100);

    bus_router_start_test(BUS_TEST_MODE_MEMORY_LEAK, SRC_UNDEFINED, TEST_SERVICE_ID, 1000, 128);
    // ... 其他测试调用 ...
    #endif // ENABLE_BUS_TESTING


    printf("=============== Infotainment System Initialized ===============\n\n");
    
    // 主函数通常在这里会进入一个低功耗循环或等待退出的逻辑
    // 在您的系统中，由于所有服务都在自己的线程中运行，
    // main 函数可能在返回后就结束了，这是正常的。
    
    return EPDK_OK;
}

