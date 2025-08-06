/**
 * @file lv_port_indev_templ.c
 *
 */

 /*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/

/*********************
 *      INCLUDES
 *********************/
#include "lv_drv_indev.h"
//#include "lv_porting.h"
#if 1
#include <gui_msg.h>
#include <msg_emit.h>
#include <log.h>
#include <mod_mcu.h>
#include <desktop_api.h>

/*********************
 *      DEFINES
 * 
 *********************/
#ifdef MIPI_TFT
#define MY_TOUCH_HOR_RES    TFT_LCD_W
#define MY_TOUCH_VER_RES    TFT_LCD_H
#endif

#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
#if defined(GX2900_1024_TOUCH) || defined(USE_GX4900_MODEL)
#define MY_DISP_HOR_RES    UI_SCREEN_W//TFT_LCD_W//800
#else
#define MY_DISP_HOR_RES    800
#endif
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen height, default value 240 is used for now.
#if defined(GX2900_1024_TOUCH) || defined(USE_GX4900_MODEL)
#define MY_DISP_VER_RES    UI_SCREEN_H//TFT_LCD_H//480
#else
#define MY_DISP_VER_RES    480
#endif
#endif


#if LV_USE_KEY
#define LONGKEY_OFFSET           (GUI_MSG_KEY_LONGUP - GUI_MSG_KEY_UP)

#ifdef DMX7018_SIMILARITY
#ifdef DMX125_TOUCH
#define     TP_INIT_KEY_NUM				(5)
#else
#ifdef GX2900_MODEL
#if GX2900_NEW_TOUCH
#define     TP_INIT_KEY_NUM				(3)
#else
#define     TP_INIT_KEY_NUM				(5)
#endif
#else
#define     TP_INIT_KEY_TY8500_NUM				(5)
#define     TP_INIT_KEY_MITS70_NUM				(3)
#endif
#endif
#endif
#endif

/**********************
 *      TYPEDEFS
 **********************/
#if LV_USE_KEY
#ifdef DMX7018_SIMILARITY
typedef struct _touch_key_table
{
	__u16 x;
	__u16 y;
	__u16 w;
	__u16 h;
	__u32 key_type;
	
}_touch_key_t_;
#endif
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
 
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
#if LV_ADD_MOVE_ACTION
static __u8 touchpad_is_pressed(void);
#else
static bool touchpad_is_pressed(void);
#endif
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

static void key_init(void);
static void key_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static uint32_t key_get_key(void);

static void mouse_init(void);
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y);

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static uint32_t keypad_get_key(void);

static void encoder_init(void);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_handler(void);

static void button_init(void);
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static int8_t button_get_pressed_id(void);
static bool button_is_pressed(uint8_t id);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad = NULL;
lv_indev_t * indev_mouse;
lv_indev_t * indev_keypad;
lv_indev_t * indev_encoder;
lv_indev_t * indev_button;

static int32_t encoder_diff;
static lv_indev_state_t encoder_state;

#if LV_USE_KEY
#ifdef DMX7018_SIMILARITY
#ifdef DMX125_TOUCH

_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	{0,50,40,70,KPAD_POWER},
	{0,130,40,70,KPAD_EQ},
	{0,220,40,70,KPAD_VOICEUP},
	{0,310,40,70,KPAD_VOICEDOWN},
	{0,400,40,80,KPAD_MUTE},
};

#else
#ifdef GX2900_MODEL
#if GX2900_NEW_TOUCH
#if GX2900_1024_TOUCH
_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	{0,130,55,30,KPAD_MUTE},
	//{0,130,45,70,KPAD_EQ},
	{0,170,55,40,KPAD_VOICEUP},
	{0,220,55,60,KPAD_VOICEDOWN},
	//{840,0,55,100,KPAD_POWER},
	//{840,110,55,100,KPAD_MENU},
	//{840,170,55,60,KPAD_IR_DISP},
};
#else
_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	{0,110,55,20,KPAD_MUTE},
	//{0,130,45,70,KPAD_EQ},
	{0,135,55,40,KPAD_VOICEUP},
	{0,180,55,60,KPAD_VOICEDOWN},
	//{840,0,55,100,KPAD_POWER},
	//{840,110,55,100,KPAD_MENU},
	//{840,170,55,60,KPAD_IR_DISP},
};
#endif
#elif GX2900_6_8_INCH_JB
_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	{0,0,45,80,KPAD_POWEROFF},
	{0,96,45,80,KPAD_VOICE},
	{0,192,45,80,KPAD_POWER},
	{0,288,45,80,KPAD_VOICEUP},
	{0,384,45,80,KPAD_VOICEDOWN},
};
#elif GX2900_6_8_INCH_HC
_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	{0,140,10,20,KPAD_POWER},
	{0,170,10,40,KPAD_POWEROFF},
	{0,210,10,40,KPAD_VOICE},
	{0,260,10,30,KPAD_VOICEUP},
	{0,295,10,20,KPAD_VOICEDOWN},
};
#elif GX2900_6_8_INCH_HC_JB
#if GX2900_6_8_INCH_JB_NEW
_touch_key_t_	init_touch_key_jb[TP_INIT_KEY_NUM]=
{
	{0,0,45,70,KPAD_MUTE},
	{0,80,45,70,KPAD_POWER},
	{0,160,45,70,KPAD_POWEROFF},
	{0,240,45,70,KPAD_VOICEUP},
	{0,320,45,70,KPAD_VOICEDOWN},
};
#else
_touch_key_t_	init_touch_key_jb[TP_INIT_KEY_NUM]=
{
	{0,0,45,80,KPAD_POWEROFF},
	{0,96,45,80,KPAD_VOICE},
	{0,192,45,80,KPAD_POWER},
	{0,288,45,80,KPAD_VOICEUP},
	{0,384,45,80,KPAD_VOICEDOWN},
};
#endif

_touch_key_t_	init_touch_key_hc[TP_INIT_KEY_NUM]=
{
	{0,140,10,20,KPAD_POWER},
	{0,170,10,40,KPAD_POWEROFF},
	{0,210,10,40,KPAD_VOICE},
	{0,260,10,30,KPAD_VOICEUP},
	{0,295,10,20,KPAD_VOICEDOWN},
};

#elif GX2900_1280_720_INCH
_touch_key_t_	init_touch_key_1280[TP_INIT_KEY_NUM]=
{
	{0,160,60,40,KPAD_MUTE},
	{0,210,60,40,KPAD_VOICEUP},
	{0,260,60,40,KPAD_VOICEDOWN},
};
#else
_touch_key_t_	init_touch_key[TP_INIT_KEY_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	{0,85,45,30,KPAD_MUTE},
	//{0,130,45,70,KPAD_EQ},
	{0,120,45,30,KPAD_VOICEUP},
	{0,162,45,40,KPAD_VOICEDOWN},
	{0,207,45,30,KPAD_POWER},
	{0,240,45,30,KPAD_POWEROFF},
};
#endif
#else
#if 1
_touch_key_t_	init_touch_TY8500_key[TP_INIT_KEY_TY8500_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	{830,120,10,40,KPAD_POWER},
	{830,170,10,40,KPAD_MENU},
	//{0,130,45,70,KPAD_EQ},
	{830,0,10,35,KPAD_MUTE},
	{830,40,10,25,KPAD_VOICEUP},
	{830,70,10,30,KPAD_VOICEDOWN},
};

#else
_touch_key_t_	init_touch_TY8500_key[TP_INIT_KEY_TY8500_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	{820,60,29,50,KPAD_POWER},
	{820,170,29,50,KPAD_MENU},
	//{0,130,45,70,KPAD_EQ},
	{851,45,851,50,KPAD_MUTE},
	{851,145,851,50,KPAD_VOICEUP},
	{851,250,851,50,KPAD_VOICEDOWN},
};
#endif
_touch_key_t_	init_touch_MITS70_key[TP_INIT_KEY_MITS70_NUM]=
{
	//{0,60,40,50,KPAD_VOICEUP},
	//{0,140,40,50,KPAD_VOICEDOWN},
	//{0,220,40,50,KPAD_MUTE},
	//{0,320,40,50,KPAD_POWEROFF},
	//{0,420,40,50,KPAD_HOME},
	//{850,60,29,50,KPAD_MUTE},
	//{850,170,29,50,KPAD_MENU},
	//{0,130,45,70,KPAD_EQ},
	{801,50,41,45,KPAD_MUTE},
	{801,145,41,45,KPAD_VOICEUP},
	{801,230,41,50,KPAD_VOICEDOWN},
};

#endif
#endif
#endif

#define INDEV_SYS_MSGQ_SIZE			32

typedef struct _indev_memit_ctr_t
{
    __hdle p_array_sem; // 缓冲区互斥
	__hdle psys_msg_queue; // msg 消息队列
	__s32  sys_msg_counter;
	__gui_msg_t sys_msg_array[INDEV_SYS_MSGQ_SIZE];	 // msg 消息队列缓冲
} __indev_memit_ctr_t;

static __indev_memit_ctr_t indev_tp_emit_ctr = {0};
static __indev_memit_ctr_t indev_key_emit_ctr = {0};
//static __krnl_event_t  * key_msg = NULL;
//static __krnl_event_t  * tp_msg = NULL;
static int touch_x = 1,touch_y = 1;
static __s32 key_type=-1;
lv_indev_t * indev_key;

#else
static __krnl_event_t  * tp_msg = NULL;
static int touch_x = 1,touch_y = 1;
#endif


/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#if LV_USE_KEY
__s32 indev_msg_init(__indev_memit_ctr_t * indev_memit)
{
	indev_memit->sys_msg_counter = 0;
	indev_memit->psys_msg_queue  = esKRNL_QCreate(INDEV_SYS_MSGQ_SIZE);

	if (!indev_memit->psys_msg_queue)
	{
		__err(" create psys_msg_queue error ");
		return EPDK_FAIL;
	}

	/* 创建 p_array_sem */
	indev_memit->p_array_sem = esKRNL_SemCreate(1);

	if (!indev_memit->p_array_sem)
	{
		__err(" create p_array_sem error ");
		return EPDK_FAIL;
	}

	return EPDK_OK;
}

static __gui_msg_t * indev_get_msg_buf(__indev_memit_ctr_t *emit)
{
    //__log("sys_msg_counter = %d ", sys_msg_counter);
    emit->sys_msg_counter   = (emit->sys_msg_counter + 1) & (INDEV_SYS_MSGQ_SIZE - 1);

    return &(emit->sys_msg_array[emit->sys_msg_counter]);
}

__s32 indev_get_msg(__hdle psys_msg_queue, __gui_msg_t *p_msg)
{
    __u8            error;
    __gui_msg_t    *tmp;

    tmp = (__gui_msg_t *)esKRNL_QAccept(psys_msg_queue, &error);
	
    if (tmp != NULL)
    {
        memcpy(p_msg, tmp, sizeof(__gui_msg_t));
        return EPDK_OK;
    }
    else
    {
        return EPDK_FAIL;
    }
}

void GUI_SendTpKeyMsg(__gui_msg_t *msg)
{
	uint8_t ret;
    uint8_t error;
	__gui_msg_t * p_msg = NULL;
	
	//printf("\nGUI_SendTpKeyMsg\n");
	esKRNL_SemPend(indev_key_emit_ctr.p_array_sem, 0, &error);
	p_msg = indev_get_msg_buf(&indev_key_emit_ctr);
	esKRNL_SemPost(indev_key_emit_ctr.p_array_sem);

	memcpy(p_msg, msg, sizeof(__gui_msg_t));
	
	ret = esKRNL_QPost(indev_key_emit_ctr.psys_msg_queue, p_msg);
	//ret = esKRNL_QPost(key_msg,msg);
	//printf("ret=%d\n", ret);
}

void GUI_SendTpMsg(__gui_msg_t *msg)
{
	uint8_t ret;
    uint8_t error;
	__gui_msg_t * p_msg = NULL;
	
	//printf("\nGUI_SendTpMsg\n");
	
	esKRNL_SemPend(indev_tp_emit_ctr.p_array_sem, 0, &error);
	p_msg = indev_get_msg_buf(&indev_tp_emit_ctr);
	esKRNL_SemPost(indev_tp_emit_ctr.p_array_sem);

	memcpy(p_msg, msg, sizeof(__gui_msg_t));
	
	ret = esKRNL_QPost(indev_tp_emit_ctr.psys_msg_queue, p_msg);
	//ret = esKRNL_QPost(tp_msg,msg);
	//printf("ret=%d\n", ret);
}
#else
void GUI_SendTpMsg(__gui_msg_t *msg)
{
	esKRNL_QPost(tp_msg,msg);
}
#endif

static __s32 tp_hook_cb(__gui_msg_t *msg)
{
	GUI_SendTpMsg(msg);

	return 0;
}

static __s32 key_hook_cb(__gui_msg_t *msg)
{
	GUI_SendTpKeyMsg(msg);

	return 0;
}



void lv_port_indev_init(void)
{
#if USE_LVGL_GUI
	if(indev_touchpad)
		return;
#endif
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    static lv_indev_drv_t indev_drv;

    /*------------------
     * Touchpad
     * -----------------*/

#if 1
    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
#endif

#if LV_USE_KEY
    /*------------------
     * Key
     * -----------------*/
     
    static lv_indev_drv_t key_indev_drv;
    /*Initialize your keypad or keyboard if you have*/
    key_init();

    /*Register a keypad input device*/
    lv_indev_drv_init(&key_indev_drv);
    key_indev_drv.type = LV_INDEV_TYPE_KEY;
    key_indev_drv.read_cb = key_read;
    indev_key = lv_indev_drv_register(&key_indev_drv);
#endif

#if 0
    /*------------------
     * Mouse
     * -----------------*/

    /*Initialize your touchpad if you have*/
    mouse_init();

    /*Register a mouse input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t * mouse_cursor = lv_img_create(lv_scr_act());
    lv_img_set_src(mouse_cursor, LV_SYMBOL_HOME);
    lv_indev_set_cursor(indev_mouse, mouse_cursor);
#endif

#if 0
    /*------------------
     * Keypad
     * -----------------*/

    /*Initialize your keypad or keyboard if you have*/
    keypad_init();

    /*Register a keypad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);
    if(indev_keypad == NULL)
    {
        __log("lv_port_indev_init failed!\n");
    }
    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/
#endif

#if 0
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    indev_encoder = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_encoder, group);`*/
#endif

#if 0
    /*------------------
     * Button
     * -----------------*/

    /*Initialize your button if you have*/
    button_init();

    /*Register a button input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_BUTTON;
    indev_drv.read_cb = button_read;
    indev_button = lv_indev_drv_register(&indev_drv);

    /*Assign buttons to points on the screen*/
    static const lv_point_t btn_points[2] = {
            {10, 10},   /*Button 0 -> x:10; y:10*/
            {40, 100},  /*Button 1 -> x:40; y:100*/
    };
    lv_indev_set_button_points(indev_button, btn_points);
#endif
	__memit_hook_t  emit_hook;
	emit_hook.key_hook		= key_hook_cb;
	emit_hook.tp_hook		= tp_hook_cb;
	msg_emit_register_hook(&emit_hook, GUI_MSG_HOOK_TYPE_KEY);
	msg_emit_register_hook(&emit_hook, GUI_MSG_HOOK_TYPE_TP);


}

/**********************
 *   STATIC FUNCTIONS
 **********************/



/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
#if LV_USE_KEY
	indev_msg_init(&indev_tp_emit_ctr);
	//tp_msg = esKRNL_QCreate(128);
	//printf("tp_msg=0x%x\n", tp_msg);
#else
	 tp_msg = esKRNL_QCreate(128);
#endif
}

#if LV_ADD_MOVE_ACTION
/*Will be called by the library to read the touchpad*/
#if SUNXI_DISP_LAYER_MODE == SUNXI_DISP_USE_MULTIPLE_LAYER
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 1;
    static lv_coord_t last_y = 1;
    __krnl_q_data_t qdata;

    OFFSET point = {1,1};
    static H_LAYER last_focus = NULL;
    static int pressing = 0;
    static int count = 0;
	__u8 indev_state = touchpad_is_pressed();
    
    /*Save the pressed coordinates and the state*/
    if((indev_state == LV_INDEV_STATE_PR) || (indev_state == LV_INDEV_STATE_MOVE)) {
        touchpad_get_xy(&last_x, &last_y);
        point.x = last_x;
        point.y = last_y;
        H_LAYER focuse = Sunxi_Lyr_Check(&point);
        if(focuse != NULL)
        {
            pressing++;
            if(pressing == 1) //记录第一次按下时的界面
            {
                Sunxi_Lyr_SetFocus(focuse);
                lyr_para_t * layer = LAYER_H2P(focuse);
                //printf("layer->name = %s\n",layer->name);

                last_focus = focuse;
            }
            else if(pressing > 1000)
            {
                pressing = 2;
            }
        }
        data->state = LV_INDEV_STATE_PR;
		if(indev_state == LV_INDEV_STATE_MOVE) {
			data->move_state = 1;
		}
    } else {

        if(pressing > 0)
        {
            OFFSET point = {0};
            point.x = last_x;
            point.y = last_y;
            H_LAYER focuse = Sunxi_Lyr_Check(&point);
            if(focuse != NULL && last_focus == focuse)
            {
                //printf("yes\n");
                Sunxi_Lyr_SetFocus(focuse);
                lyr_para_t * layer = LAYER_H2P(focuse);
                last_focus = NULL;
            }
            data->state = LV_INDEV_STATE_REL;
            count = 0;
            pressing = 0;
        }
    }

	esKRNL_QQuery(indev_tp_emit_ctr.psys_msg_queue, &qdata);
	if(qdata.OSNMsgs != 0)
	{
		data->continue_reading = true;
	}

    /*Set the last pressed coordinates*/
    data->point.x = point.x;
    data->point.y = point.y;
	//printf("point:x=%d, y=%d\n",data->point.x, data->point.y);

    /*Return `false` because we are not buffering and no more data to read*/
    return false;
}
#else
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 1;
    static lv_coord_t last_y = 1;
    __krnl_q_data_t qdata;

	__u8 indev_state = touchpad_is_pressed();
	
    /*Save the pressed coordinates and the state*/
    if((indev_state == LV_INDEV_STATE_PR) || (indev_state == LV_INDEV_STATE_MOVE)) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
        if(indev_state == LV_INDEV_STATE_MOVE) {
        	data->move_state = 1;
        }
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

	esKRNL_QQuery(indev_tp_emit_ctr.psys_msg_queue, &qdata);
	if(qdata.OSNMsgs != 0)
	{
		data->continue_reading = true;
	}

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}
#endif

/*Return true is the touchpad is pressed*/
static __u8 touchpad_is_pressed(void)
{
    /*Your code comes here*/
	__u8 error; 
	__gui_msg_t *msg;
    
	static __u8 pressed_last = LV_INDEV_STATE_REL;

	msg = (__gui_msg_t *)esKRNL_QAccept(indev_tp_emit_ctr.psys_msg_queue, &error);
	//msg = (__gui_msg_t *)esKRNL_QAccept( tp_msg, &error);

	if(msg != NULL)
	{
		reg_root_para_t* para;
		
		para = (reg_root_para_t*)dsk_reg_get_para_by_app(REG_APP_ROOT);
		if(para)
			para->demo_time_cnt = 0;	
		//printf("msg->dwAddData1=%d\n", msg->dwAddData1);
		if(msg->dwAddData1 == GUI_MSG_TOUCH_DOWN)
		{

			touch_x = LOSWORD(msg->dwAddData2) - TOUCH_OFFSET;
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>MY_DISP_HOR_RES)touch_x=MY_DISP_HOR_RES;
			if(touch_y>MY_DISP_VER_RES)touch_y=MY_DISP_VER_RES;
			//printf("\nDOWN x = %d y = %d\n",touch_x,touch_y);
			pressed_last = LV_INDEV_STATE_PR;
			return LV_INDEV_STATE_PR;
		}
		else if(msg->dwAddData1 == GUI_MSG_TOUCH_MOVE)
		{

			touch_x = LOSWORD(msg->dwAddData2) - TOUCH_OFFSET;
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>MY_DISP_HOR_RES)touch_x=MY_DISP_HOR_RES;
			if(touch_y>MY_DISP_VER_RES)touch_y=MY_DISP_VER_RES;
			//printf("\nMOVE x = %d y = %d\n",touch_x,touch_y);
			pressed_last = LV_INDEV_STATE_MOVE;
			return LV_INDEV_STATE_MOVE;
		}
		else
		{
			touch_x = LOSWORD(msg->dwAddData2) - TOUCH_OFFSET;
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>MY_DISP_HOR_RES)touch_x=MY_DISP_HOR_RES;
			if(touch_y>MY_DISP_VER_RES)touch_y=MY_DISP_VER_RES;
			//printf("\nUP x = %d y = %d\n",touch_x,touch_y);
			pressed_last = LV_INDEV_STATE_REL;
			return LV_INDEV_STATE_REL;
		}
	}
	else
	{
		//printf("pressed_last=%d\n", pressed_last);
		return pressed_last;
	}

}
#else
#if SUNXI_DISP_LAYER_MODE == SUNXI_DISP_USE_MULTIPLE_LAYER

/* Will be called by the library to read the touchpad */
static bool touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    OFFSET point = {0};
    static H_LAYER last_focus = NULL;
    static int pressing = 0;
    static int count = 0;
    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        point.x = last_x;
        point.y = last_y;
        H_LAYER focuse = Sunxi_Lyr_Check(&point);
        if(focuse != NULL)
        {
            pressing++;
            if(pressing == 1) //记录第一次按下时的界面
            {
                Sunxi_Lyr_SetFocus(focuse);
                lyr_para_t * layer = LAYER_H2P(focuse);
                __log("layer->name = %s",layer->name);

                last_focus = focuse;
            }
            else if(pressing > 1000)
            {
                pressing = 2;
            }
        }
        data->state = LV_INDEV_STATE_PR;
    } else {

        if(pressing > 0)
        {
            OFFSET point = {0};
            point.x = last_x;
            point.y = last_y;
            H_LAYER focuse = Sunxi_Lyr_Check(&point);
            if(focuse != NULL && last_focus == focuse)
            {
                __log("yes");
                Sunxi_Lyr_SetFocus(focuse);
                lyr_para_t * layer = LAYER_H2P(focuse);
                last_focus = NULL;
            }
            data->state = LV_INDEV_STATE_REL;
            count = 0;
            pressing = 0;
        }
    }

    /*Set the last pressed coordinates*/
    data->point.x = point.x;
    data->point.y = point.y;

    /*Return `false` because we are not buffering and no more data to read*/
    return false;
}

#else
static bool touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;

    /*Return `false` because we are not buffering and no more data to read*/
    return false;
}
#endif

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    /*Your code comes here*/
	#if LV_USE_KEY
	__u8 error; 
	__gui_msg_t *msg;
	
	static bool pressed_last = false;

	msg = (__gui_msg_t *)esKRNL_QAccept(indev_tp_emit_ctr.psys_msg_queue, &error);
	//msg = (__gui_msg_t *)esKRNL_QAccept( tp_msg, &error);
	
	if(msg != NULL)
	{
		reg_root_para_t* para;
		
		para = (reg_root_para_t*)dsk_reg_get_para_by_app(REG_APP_ROOT);
		if(para)
			para->demo_time_cnt = 0;	
		//printf("msg->dwAddData1=%d\n", msg->dwAddData1);
		if(msg->dwAddData1 == GUI_MSG_TOUCH_DOWN || msg->dwAddData1 == GUI_MSG_TOUCH_MOVE)
		{

			touch_x = LOSWORD(msg->dwAddData2) - TOUCH_OFFSET;
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>MY_DISP_HOR_RES)touch_x=MY_DISP_HOR_RES;
			if(touch_y>MY_DISP_VER_RES)touch_y=MY_DISP_VER_RES;
			//eLIBs_printf("touch1 x = %d y = %d\n",touch_x,touch_y);
			pressed_last = true;
            //esKRNL_QFlush(tp_msg);
			return true;
		}
		else
		{
			touch_x = LOSWORD(msg->dwAddData2) - TOUCH_OFFSET;
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>MY_DISP_HOR_RES)touch_x=MY_DISP_HOR_RES;
			if(touch_y>MY_DISP_VER_RES)touch_y=MY_DISP_VER_RES;
			//eLIBs_printf("touch2 x = %d y = %d\n",touch_x,touch_y);
			pressed_last = false;
            //esKRNL_QFlush(tp_msg);
			return false;
		}
	}
	else
	{
		return pressed_last;
	}
	#else
	__u8 error;
	__gui_msg_t *msg;

	static bool pressed_last = false;

	msg = (__gui_msg_t *)esKRNL_QAccept(tp_msg, &error);
	if(msg != NULL)
	{
		if(msg->dwAddData1 == GUI_MSG_TOUCH_DOWN || msg->dwAddData1 == GUI_MSG_TOUCH_MOVE)
		{
			touch_x = LOSWORD(msg->dwAddData2);
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>lv_get_screen_width())touch_x=lv_get_screen_width();
			if(touch_y>lv_get_screen_height())touch_y=lv_get_screen_height();

			pressed_last = true;
            esKRNL_QFlush(tp_msg);
			return true;
		}
		else
		{
			touch_x = LOSWORD(msg->dwAddData2);
			touch_y = HISWORD(msg->dwAddData2);
			if(touch_x>lv_get_screen_width())touch_x=lv_get_screen_width();
			if(touch_y>lv_get_screen_height())touch_y=lv_get_screen_height();
			// eLIBs_printf("touch2 x = %d y = %d\n",touch_x,touch_y);
			pressed_last = false;
            esKRNL_QFlush(tp_msg);
			return false;
		}
	}
	else
	{
		return pressed_last;
	}
    return false;
#endif
}
#endif

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/
	(*x) = touch_x;
	(*y) = touch_y;
}

#if LV_USE_KEY
/*------------------
 * Key
 * -----------------*/
 
/*Initialize your key*/
static void key_init(void)
{
    /*Your code comes here*/
	indev_msg_init(&indev_key_emit_ctr);
	//key_msg = esKRNL_QCreate(128);
	//printf("key_msg=0x%x\n", key_msg);
}

/*Will be called by the library to read the mouse*/
static void key_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;
    __krnl_q_data_t qdata;

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = key_get_key();
    if(act_key != 0) {
        data->state = LV_INDEV_STATE_PR;

        /*Translate the keys to LVGL control characters according to your key definitions*/
        /*switch(key_type) {
            case KPAD_MUTE:
                act_key = LV_KEY_MUTE;
                break;
            case KPAD_VOICEUP:
                act_key = LV_KEY_VOLUP;
                break;
            case KPAD_VOICEDOWN:
                act_key = LV_KEY_VOLDOWN;
                break;
			case KPAD_POWER:
				act_key = LV_KEY_POWER;
				break;
			case KPAD_POWEROFF:
				act_key = LV_KEY_POWER_OFF;
				break;
			default:
				act_key = key_type;
			break;
        }*/
		act_key = key_type;
        last_key = act_key;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
	esKRNL_QQuery(indev_key_emit_ctr.psys_msg_queue, &qdata);
	if(qdata.OSNMsgs != 0)
	{
		data->continue_reading = true;
	}
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t key_get_key(void)
{
    /*Your code comes here*/
#if 1
	__u8 error; 
	__gui_msg_t *msg;
	__u8 i;
	static __s32 last_key=-1;
	static __s32 last_key_function=-1;
	static bool pressed_last = false;
	__pos_t pos;
#if GX2900_6_8_INCH_HC_JB
	reg_system_para_t* system_para;
	
	system_para = (reg_system_para_t*)dsk_reg_get_para_by_app(REG_APP_SYSTEM);
#endif
	msg = (__gui_msg_t *)esKRNL_QAccept(indev_key_emit_ctr.psys_msg_queue, &error);
	//msg = (__gui_msg_t *)esKRNL_QAccept(key_msg, &error);
	
	if(msg != NULL)
	{
		if(msg->id == GUI_MSG_TOUCH)
		{
			//printf("key:msg->dwAddData1=%d\n", msg->dwAddData1);
#if (defined(MIPI_TFT) && !defined(GX2900_MODEL))

			if(msg->dwAddData1 == GUI_MSG_TOUCH_DOWN || msg->dwAddData1 == GUI_MSG_TOUCH_MOVE)
			{

				pos.x = LOSWORD(msg->dwAddData2);
				pos.y = HISWORD(msg->dwAddData2);
				//printf("0:pos.x = %d y = %d\n",pos.x,pos.y);
				//if(pos.x>MY_TOUCH_HOR_RES)pos.x=MY_TOUCH_HOR_RES;
				//if(pos.y>MY_TOUCH_VER_RES)pos.y=MY_TOUCH_VER_RES;
				//printf("1:pos.x = %d y = %d\n",pos.x,pos.y);
				pressed_last = true;
				//return true;
			}
			else
			{
				pos.x = LOSWORD(msg->dwAddData2);
				pos.y = HISWORD(msg->dwAddData2);
				//printf("00:pos.x = %d y = %d\n",pos.x,pos.y);
				//if(pos.x>MY_TOUCH_HOR_RES)pos.x=MY_TOUCH_HOR_RES;
				//if(pos.y>MY_TOUCH_VER_RES)pos.y=MY_TOUCH_VER_RES;
				//printf("11:pos.x = %d y = %d\n",pos.x,pos.y);
				pressed_last = false;
				//return false;
			}
			if(pos.x>MY_DISP_HOR_RES)
			{
				reg_system_para_t*system_para;
				
				system_para = (reg_system_para_t*)dsk_reg_get_para_by_app(REG_APP_SYSTEM);
				if(system_para->tft_select==1)
				{
					for(i=0;i<TP_INIT_KEY_MITS70_NUM;i++)
					{
						if((pos.x>=init_touch_MITS70_key[i].x && pos.x<=(init_touch_MITS70_key[i].x+init_touch_MITS70_key[i].w))
						&& (pos.y>=init_touch_MITS70_key[i].y && pos.y<=(init_touch_MITS70_key[i].y+init_touch_MITS70_key[i].h)))
						{
							key_type = init_touch_MITS70_key[i].key_type;
							//printf("0:key_type = %d\n",key_type);
							break;
						}
					}
				}
				else
				{
					for(i=0;i<TP_INIT_KEY_TY8500_NUM;i++)
					{
						if((pos.x>=init_touch_TY8500_key[i].x && pos.x<=(init_touch_TY8500_key[i].x+init_touch_TY8500_key[i].w))
						&& (pos.y>=init_touch_TY8500_key[i].y && pos.y<=(init_touch_TY8500_key[i].y+init_touch_TY8500_key[i].h)))
						{
							key_type = init_touch_TY8500_key[i].key_type;
							//printf("1:key_type = %d\n",key_type);
							break;
						}
					}
				}
			}
#else
			if(msg->dwAddData1 == GUI_MSG_TOUCH_DOWN || msg->dwAddData1 == GUI_MSG_TOUCH_MOVE)
			{
			
				pos.x = LOSWORD(msg->dwAddData2);
				pos.y = HISWORD(msg->dwAddData2);
				if(pos.x>MY_DISP_HOR_RES)pos.x=MY_DISP_HOR_RES;
				if(pos.y>MY_DISP_VER_RES)pos.y=MY_DISP_VER_RES;
				//printf("pos.x = %d y = %d\n",pos.x,pos.y);
				pressed_last = true;
				//return true;
			}
			else
			{
				pos.x = LOSWORD(msg->dwAddData2);
				pos.y = HISWORD(msg->dwAddData2);
				if(pos.x>MY_DISP_HOR_RES)pos.x=MY_DISP_HOR_RES;
				if(pos.y>MY_DISP_VER_RES)pos.y=MY_DISP_VER_RES;
				//printf("pos.x = %d y = %d\n",pos.x,pos.y);
				pressed_last = false;
				//return false;
			}
			if(pos.x<TOUCH_OFFSET)
			{
				for(i=0;i<TP_INIT_KEY_NUM;i++)
				{
#if GX2900_6_8_INCH_HC_JB
					if(system_para->touch_select_flag)
					{
						if((pos.x>=init_touch_key_hc[i].x && pos.x<=(init_touch_key_hc[i].x+init_touch_key_hc[i].w))
						&& (pos.y>=init_touch_key_hc[i].y && pos.y<=(init_touch_key_hc[i].y+init_touch_key_hc[i].h)))
						{
							key_type = init_touch_key_hc[i].key_type;
							//printf("key_type = %d\n",key_type);
							break;
						}
					}
					else
					{
						if((pos.x>=init_touch_key_jb[i].x && pos.x<=(init_touch_key_jb[i].x+init_touch_key_jb[i].w))
						&& (pos.y>=init_touch_key_jb[i].y && pos.y<=(init_touch_key_jb[i].y+init_touch_key_jb[i].h)))
						{
							key_type = init_touch_key_jb[i].key_type;
							//printf("key_type = %d\n",key_type);
							break;
						}
					}
#elif GX2900_1280_720_INCH
					if((pos.x>=init_touch_key_1280[i].x && pos.x<=(init_touch_key_1280[i].x+init_touch_key_1280[i].w))
					&& (pos.y>=init_touch_key_1280[i].y && pos.y<=(init_touch_key_1280[i].y+init_touch_key_1280[i].h)))
					{
						key_type = init_touch_key_1280[i].key_type;
						//printf("key_type = %d\n",key_type);
						break;
					}
#else
					if((pos.x>=init_touch_key[i].x && pos.x<=(init_touch_key[i].x+init_touch_key[i].w))
					&& (pos.y>=init_touch_key[i].y && pos.y<=(init_touch_key[i].y+init_touch_key[i].h)))
					{
						key_type = init_touch_key[i].key_type;
						//printf("key_type = %d\n",key_type);
						break;
					}
#endif
				}
				if(i >= TP_INIT_KEY_NUM)
				{
					pressed_last = false;
				}
			}
#endif
		}
		else if(msg->id == GUI_MSG_KEY)
		{
			if(msg->dwAddData2 == KEY_REPEAT_ACTION)
			{
				msg->dwAddData1 += LONGKEY_OFFSET;
			}
			if(msg->dwAddData2 == 0)
			{
				pressed_last = false;
			}
			else
			{
				pressed_last = true;
			}

			key_type = msg->dwAddData1;
		}
	}
	
	//printf("pressed_last=%d\n", pressed_last);
	return pressed_last;

#else
    return 0;
#endif
}

#endif

/*------------------
 * Mouse
 * -----------------*/

/* Initialize your mouse */
static void mouse_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the mouse*/
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the mouse button is pressed or released*/
    if(mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*Return true is the mouse button is pressed*/
static bool mouse_is_pressed(void)
{
    /*Your code comes here*/

    return false;
}

/*Get the x and y coordinates if the mouse is pressed*/
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/

    (*x) = 0;
    (*y) = 0;
}

/*------------------
 * Keypad
 * -----------------*/

/* Initialize your keypad */
static void keypad_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the mouse*/
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;

    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = keypad_get_key();
    if(act_key != 0) {
        data->state = LV_INDEV_STATE_PR;

        /*Translate the keys to LittlevGL control characters according to your key definitions*/
        switch(act_key) {
        case 1:
            act_key = LV_KEY_NEXT;
            break;
        case 2:
            act_key = LV_KEY_PREV;
            break;
        case 3:
            act_key = LV_KEY_LEFT;
            break;
        case 4:
            act_key = LV_KEY_RIGHT;
            break;
        case 5:
            act_key = LV_KEY_ENTER;
            break;
        }

        last_key = act_key;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
    /*Your code comes here*/

    return 0;
}

/*------------------
 * Encoder
 * -----------------*/

/* Initialize your keypad */
static void encoder_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    data->enc_diff = encoder_diff;
    data->state = encoder_state;
}

/*Call this function in an interrupt to process encoder events (turn, press)*/
static void encoder_handler(void)
{
    /*Your code comes here*/

    encoder_diff += 0;
    encoder_state = LV_INDEV_STATE_REL;
}


/*------------------
 * Button
 * -----------------*/

/* Initialize your buttons */
static void button_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the button*/
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

    static uint8_t last_btn = 0;

    /*Get the pressed button's ID*/
    int8_t btn_act = button_get_pressed_id();

    if(btn_act >= 0) {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Save the last pressed button's ID*/
    data->btn_id = last_btn;
}

/*Get ID  (0, 1, 2 ..) of the pressed button*/
static int8_t button_get_pressed_id(void)
{
    uint8_t i;

    /*Check to buttons see which is being pressed (assume there are 2 buttons)*/
    for(i = 0; i < 2; i++) {
        /*Return the pressed button's ID*/
        if(button_is_pressed(i)) {
            return i;
        }
    }

    /*No button pressed*/
    return -1;
}

/*Test if `id` button is pressed or not*/
static bool button_is_pressed(uint8_t id)
{

    /*Your code comes here*/

    return false;
}

#else /* Enable this file at the top */

/* This dummy typedef exists purely to silence -Wpedantic. */
typedef int keep_pedantic_happy;
#endif
