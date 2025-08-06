#include "ui_manger.h"
#include "ui_clock.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>

#define M_PI (3.141592654)

typedef struct {
	lv_obj_t* container;			// 控件的主容器
	lv_obj_t * ui_clock_pancel;
	
	lv_obj_t * ui_clock_bmp;
	lv_obj_t * ui_clock_face;
	lv_obj_t * ui_meter;
	lv_meter_indicator_t * ui_indic_sec;
	lv_meter_indicator_t * ui_indic_min;
	lv_meter_indicator_t * ui_indic_hour;

	lv_timer_t * ui_clock_timer;
	
	lv_obj_t * ui_clock_text_pancel;
	lv_obj_t * ui_hour_text;
	lv_obj_t * ui_min_text;
	lv_obj_t * ui_time_text_pancel;
	lv_obj_t * ui_month_text;
	lv_obj_t * ui_day_text;
	lv_obj_t * ui_year_text;

	
	HTHEME lv_clock_bmp[MAX_CLOCK_ITEM];
	lv_img_dsc_t lv_clock_icon[MAX_CLOCK_ITEM];

} clock_ui_t;


static clock_ui_t g_clock_state;
static __awos_time_t clock_current_time;






static lv_obj_t* clock_widget_create(lv_obj_t* parent, void* arg);
static void clock_widget_destroy(ui_manger_t* widget);
static int clock_widget_handle_event(ui_manger_t* widget, const app_message_t* msg);


lv_img_dsc_t * ui_clock_get_res(clock_ui_t * clock_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_clock_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_CLOCK_ITEM)
	{
		__err("error MAX_source_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_CLOCK_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_clock_res_bmp[icon] == NULL) || icon > (MAX_CLOCK_ITEM - 1))
		return NULL;

	if(clock_para->lv_clock_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_clock_res_bmp[icon], &clock_para->lv_clock_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_clock_res_bmp[icon], &clock_para->lv_clock_bmp[icon], &clock_para->lv_clock_icon[icon]);
	}

	return &clock_para->lv_clock_icon[icon];
}

void ui_clock_uninit_res(clock_ui_t * clock_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_CLOCK_ITEM; i++ )
	{
		if( clock_para->lv_clock_bmp[i] )
		{
			dsk_theme_close( clock_para->lv_clock_bmp[i] );
			clock_para->lv_clock_bmp[i] = NULL;
		}
	}
}





// --- 全局变量 ---
// 定义这个控件的操作函数集
static const ui_manger_ops_t clock_ops = {
    .create = clock_widget_create,
    .destroy = clock_widget_destroy,
    .handle_event = clock_widget_handle_event,
    // .show 和 .hide 可以使用默认实现或自定义
};


// --- 公共函数 ---
/**
 * @brief 向UI管理器注册Radio控件
 */
void ui_clock_register_init(void) {
    ui_manager_register(MANGER_ID_CLOCK, &clock_ops);
}

static __awos_time_t * ui_clock_get_time(void)
{
	reg_system_para_t* sys_para;
	__awos_time_t * time;
	sys_para = (reg_system_para_t*)dsk_reg_get_para_by_app(REG_APP_SYSTEM);

	time = &sys_para->time;
	//eLIBs_printf("hour=%d\n", time->hour);
	//eLIBs_printf("minute=%d\n", time->minute);
	//eLIBs_printf("second=%d\n", time->second);
	
	return time;
}

__s32 ui_clock_timer(clock_ui_t * clock_para)
{


	__awos_time_t *time = ui_clock_get_time();
	static __u8 sec = 0;
	char * p1 = (char *)&clock_current_time;
	char * p2 = (char *)time;
	char * time_text;
	char * date_text;
	//eLIBs_printf("1:hour=%d\n", clock_current_time.hour);
	//eLIBs_printf("1:minute=%d\n", clock_current_time.minute);
	//eLIBs_printf("1:second=%d\n", clock_current_time.second);
	if(eLIBs_memcmp(p1, p2, sizeof(__awos_time_t)) == 0)
	{
		
		sec = 0;
		lv_meter_set_indicator_end_value(clock_para->ui_meter, clock_para->ui_indic_min, time->minute);
		if(clock_current_time.hour != time->hour)
		{
			lv_meter_set_indicator_end_value(clock_para->ui_meter, clock_para->ui_indic_hour, time->hour);
		}
		eLIBs_memcpy(&clock_current_time, time, sizeof(__awos_time_t));
	}
	//__home_msg("sec = %d\n", sec);
	lv_meter_set_indicator_end_value(clock_para->ui_meter, clock_para->ui_indic_sec, sec);
	sec++;

	return EPDK_OK;
}

static void ui_event_clock_timer_cb(lv_timer_t * t)
{	
	clock_ui_t * clock_para = t->user_data;

#if 0//LV_F133_USE_ASYNC_THREAD
	memset(&clock_para->clock_msg, 0x00, sizeof(__gui_msg_t));
	clock_para->clock_msg.id = GUI_MSG_BACKCAR_OPN_TIMER;
	ui_clock_opn_msg(clock_para);
#else
	ui_clock_timer(clock_para);
#endif
}


#if 1
static lv_obj_t * ui_clock_set_meter(clock_ui_t * clock_para)
{
	lv_obj_t * ui_home_meter = clock_para->ui_meter;
	
	//lv_obj_remove_style_all(ui_home_meter);
    //lv_obj_set_size(ui_home_meter, 230, 230);
    //lv_obj_set_x(ui_home_meter, lv_pct(0));
    //lv_obj_set_y(ui_home_meter, lv_pct(8));
    //lv_obj_set_align(ui_home_meter, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(ui_home_meter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_set_style_bg_img_src(ui_home_meter, ui_clock_get_res(clock_para, CLOCK_CLOCK_BG_BMP), LV_PART_MAIN | LV_STATE_DEFAULT);

    /*Create a scale for the second*/
    /*61 ticks in a 360 degrees range (the last and the first line overlaps)*/
    lv_meter_scale_t * scale_sec = lv_meter_add_scale(ui_home_meter);
    lv_meter_set_scale_ticks(ui_home_meter, scale_sec, 61, 0, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(ui_home_meter, scale_sec, 0, 60, 360, 0);

    /*Create a scale for the minutes*/
    /*61 ticks in a 360 degrees range (the last and the first line overlaps)*/
    lv_meter_scale_t * scale_min = lv_meter_add_scale(ui_home_meter);
    lv_meter_set_scale_ticks(ui_home_meter, scale_min, 61, 0, 15, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(ui_home_meter, scale_min, 0, 60, 360, 0);

    /*Create another scale for the hours. It's only visual and contains only major ticks*/
    lv_meter_scale_t * scale_hour = lv_meter_add_scale(ui_home_meter);
    lv_meter_set_scale_ticks(ui_home_meter, scale_hour, 12, 2, 15, lv_color_hex(0xffffff));//lv_color_white()               /*12 ticks*/
    //lv_meter_set_scale_major_ticks(meter, scale_hour, 1, 2, 20, lv_palette_main(LV_PALETTE_GREEN), 10);    /*Every tick is major*/
    lv_meter_set_scale_range(ui_home_meter, scale_hour, 1, 12, 330, 180);       /*[1..12] values in an almost full circle*/

    /*Add a the hands from images*/
	
	//ui_clock_meter_para->ui_indic_sec = lv_meter_add_needle_line(ui_clock_home_meter, scale_sec, 2, lv_palette_main(LV_PALETTE_RED), 0);
    clock_para->ui_indic_sec = lv_meter_add_needle_line(ui_home_meter, scale_sec, 2, lv_color_hex(0x00ff00),-3);
    clock_para->ui_indic_min = lv_meter_add_needle_line(ui_home_meter, scale_min, 3, lv_color_hex(0xffffff),-18);
   clock_para->ui_indic_hour = lv_meter_add_needle_line(ui_home_meter, scale_hour, 6, lv_color_hex(0xffffff),-30);

#if 0
    /*Create an animation to set the value*/
    lv_anim_init(&clock_anim);
    lv_anim_set_exec_cb(&clock_anim, set_value);
    lv_anim_set_values(&clock_anim, 0, 60);
    lv_anim_set_repeat_count(&clock_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_time(&clock_anim, 60000);     /*2 sec for 1 turn of the minute hand (1 hour)*/
    lv_anim_set_var(&clock_anim, ui_home_indic_sec);
    lv_anim_start(&clock_anim);
#endif
	return ui_home_meter;
}
#endif



// --- 静态函数实现 ---

/**
 * @brief 创建Radio主界面的所有LVGL对象
 */
static lv_obj_t* clock_widget_create(lv_obj_t* parent, void* arg) {
    if (!parent) return NULL;
	memset(&g_clock_state, 0, sizeof(g_clock_state));
    g_clock_state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_clock_state.container);
    lv_obj_set_size(g_clock_state.container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_clock_state.container, LV_OBJ_FLAG_SCROLLABLE);

      g_clock_state.ui_clock_pancel = lv_obj_create(g_clock_state.container);
    lv_obj_set_width(g_clock_state.ui_clock_pancel, 390);
    lv_obj_set_height(g_clock_state.ui_clock_pancel, 312);
    lv_obj_set_x(g_clock_state.ui_clock_pancel, 0);
    lv_obj_set_y(g_clock_state.ui_clock_pancel, 0);
    lv_obj_clear_flag(g_clock_state.ui_clock_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_clock_state.ui_clock_pancel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_clock_state.ui_clock_pancel, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_clock_state.ui_clock_pancel, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_clock_state.ui_clock_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


#if 1
	 g_clock_state.ui_clock_face = lv_obj_create(g_clock_state.ui_clock_pancel);
    lv_obj_set_size(g_clock_state.ui_clock_face, 220, 220);
    //lv_obj_center(g_clock_state.ui_clock_face);
     lv_obj_set_x(g_clock_state.ui_clock_face, 28);
    lv_obj_set_y(g_clock_state.ui_clock_face, 29);
    lv_obj_clear_flag(g_clock_state.ui_clock_face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(g_clock_state.ui_clock_face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_clock_state.ui_clock_face, lv_color_hex(0000), 0);
    lv_obj_set_style_border_color(g_clock_state.ui_clock_face, lv_color_hex(0000), 0);
	lv_obj_set_style_border_opa(g_clock_state.ui_clock_face, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_clock_state.ui_clock_face, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


	 g_clock_state.ui_meter = lv_meter_create(g_clock_state.ui_clock_face);
	
    lv_obj_set_size(g_clock_state.ui_meter, 220, 220);
	
    lv_obj_set_align(g_clock_state.ui_meter, LV_ALIGN_CENTER);
	ui_clock_set_meter(&g_clock_state);



#else



	   g_clock_state.ui_clock_bmp = lv_img_create(g_clock_state.ui_clock_pancel);
    lv_img_set_src(g_clock_state.ui_clock_bmp, ui_clock_get_res(&g_clock_state,CLOCK_IMAGE_BMP));
    lv_obj_set_width(g_clock_state.ui_clock_bmp, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_clock_bmp, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_clock_state.ui_clock_bmp, 28);
    lv_obj_set_y(g_clock_state.ui_clock_bmp, 29);
    lv_obj_add_flag(g_clock_state.ui_clock_bmp, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_clock_state.ui_clock_bmp, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
#endif
    g_clock_state.ui_clock_text_pancel = lv_obj_create(g_clock_state.ui_clock_pancel);
    lv_obj_set_width(g_clock_state.ui_clock_text_pancel, 80);
    lv_obj_set_height(g_clock_state.ui_clock_text_pancel, 125);
    lv_obj_set_x(g_clock_state.ui_clock_text_pancel, 265);
    lv_obj_set_y(g_clock_state.ui_clock_text_pancel, 75);
    lv_obj_set_flex_flow(g_clock_state.ui_clock_text_pancel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_clock_state.ui_clock_text_pancel, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_clock_state.ui_clock_text_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_clock_state.ui_clock_text_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_clock_state.ui_clock_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_hour_text = lv_label_create(g_clock_state.ui_clock_text_pancel);
    lv_obj_set_width(g_clock_state.ui_hour_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_hour_text, 50);    /// 1
    lv_obj_set_x(g_clock_state.ui_hour_text, 280);
    lv_obj_set_y(g_clock_state.ui_hour_text, 61);
    lv_label_set_text(g_clock_state.ui_hour_text, "15");
	  lv_obj_set_style_text_color(g_clock_state.ui_hour_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_clock_state.ui_hour_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_clock_state.ui_hour_text, lv_font_xlarge.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_min_text = lv_label_create(g_clock_state.ui_clock_text_pancel);
    lv_obj_set_width(g_clock_state.ui_min_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_min_text, 50);    /// 1
    lv_obj_set_x(g_clock_state.ui_min_text, 280);
    lv_obj_set_y(g_clock_state.ui_min_text, 115);
    lv_label_set_text(g_clock_state.ui_min_text, "37");
    lv_obj_set_style_text_color(g_clock_state.ui_min_text, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_clock_state.ui_min_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_clock_state.ui_min_text, lv_font_xlarge.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_time_text_pancel = lv_obj_create(g_clock_state.ui_clock_pancel);
    lv_obj_set_width(g_clock_state.ui_time_text_pancel, 200);
    lv_obj_set_height(g_clock_state.ui_time_text_pancel, 40);
    lv_obj_set_x(g_clock_state.ui_time_text_pancel, 110);
    lv_obj_set_y(g_clock_state.ui_time_text_pancel, 255);
    lv_obj_set_flex_flow(g_clock_state.ui_time_text_pancel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_clock_state.ui_time_text_pancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_clock_state.ui_time_text_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_clock_state.ui_time_text_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_clock_state.ui_time_text_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_month_text = lv_label_create(g_clock_state.ui_time_text_pancel);
    lv_obj_set_width(g_clock_state.ui_month_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_month_text, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(g_clock_state.ui_month_text, "Sep");
    lv_obj_set_style_text_color(g_clock_state.ui_month_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_clock_state.ui_month_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_clock_state.ui_month_text, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_day_text = lv_label_create(g_clock_state.ui_time_text_pancel);
    lv_obj_set_width(g_clock_state.ui_day_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_day_text, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(g_clock_state.ui_day_text, "24,");
    lv_obj_set_style_text_color(g_clock_state.ui_day_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_clock_state.ui_day_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_clock_state.ui_day_text, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_clock_state.ui_year_text = lv_label_create(g_clock_state.ui_time_text_pancel);
    lv_obj_set_width(g_clock_state.ui_year_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_clock_state.ui_year_text, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(g_clock_state.ui_year_text, "2025");
    lv_obj_set_style_text_color(g_clock_state.ui_year_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_clock_state.ui_year_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_clock_state.ui_year_text, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

	
    g_clock_state.ui_clock_timer = lv_timer_create(ui_event_clock_timer_cb, 100*10, &g_clock_state);
   

    return g_clock_state.container;
}

/**
 * @brief 销毁Radio控件时释放私有数据
 */
static void clock_widget_destroy(ui_manger_t* widget) {
     if (widget && widget->instance) {
        lv_obj_del(widget->instance);
    }
	 if(g_clock_state.ui_clock_timer)
	{
		lv_timer_del(g_clock_state.ui_clock_timer);
		g_clock_state.ui_clock_timer = NULL;
	}
ui_clock_uninit_res(&g_clock_state)	; 
    memset(&g_clock_state, 0, sizeof(g_clock_state));
}

/**
 * @brief 处理来自服务端的事件，并更新UI
 */
static int clock_widget_handle_event(ui_manger_t* widget, const app_message_t* msg) {
    if (!widget || !widget->instance || !msg) return -1;

    // 从 widget->instance (即cont) 中获取私有数据
    

    // 将通用载荷转换为Radio专用载荷
    //const msg_payload_service_radio_t* payload = (const msg_payload_service_radio_t*)msg->payload;

    switch (msg->command) {

        default:
            // 其他不关心的消息，直接忽略
            break;
    }
    return 0;
}



