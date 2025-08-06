#include "ui_manger.h"
#include "ui_sys_control.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>


typedef struct {
	lv_obj_t* container;			// 控件的主容器
	lv_obj_t * ui_system_control_pancel;
	lv_obj_t * ui_volume_control_pancel;
	lv_obj_t * ui_volum_pancel;
	lv_obj_t * ui_volume_icon;
	lv_obj_t * ui_volume_val;
	lv_obj_t * ui_volume_slider;
	lv_obj_t * ui_backlight_control_pancel;
	lv_obj_t * ui_backlight_pancel;
	lv_obj_t * ui_backlight_icon;
	lv_obj_t * ui_backlight_text;
	lv_obj_t * ui_backlight_slider;
	lv_obj_t * ui_sys_control_bottom_pancel;
	lv_obj_t * ui_sel_phone_pancel;
	lv_obj_t * ui_sel_phone_bg;
	lv_obj_t * ui_phone_button;
	lv_obj_t * ui_phone_text;
	lv_obj_t * ui_sel_audio_pancel;
	lv_obj_t * ui_sel_audio_bg;
	lv_obj_t * ui_audio_button;
	lv_obj_t * ui_audio_text;
	lv_obj_t * ui_sel_can_pancel;
	lv_obj_t * ui_sel_can_bg;
	lv_obj_t * ui_can_button;
	lv_obj_t * ui_can_text;

	HTHEME lv_sys_control_bmp[MAX_SYS_CONTROL_ITEM];
	lv_img_dsc_t lv_sys_control_icon[MAX_SYS_CONTROL_ITEM];
} sys_control_ui_t;

static sys_control_ui_t g_sys_control_state;


static lv_obj_t* sys_control_widget_create(lv_obj_t* parent, void* arg);
static void sys_control_widget_destroy(ui_manger_t* widget);
static int sys_control_widget_handle_event(ui_manger_t* widget, const app_message_t* msg);


// --- 全局变量 ---
// 定义这个控件的操作函数集
static const ui_manger_ops_t sys_control_ops = {
    .create = sys_control_widget_create,
    .destroy = sys_control_widget_destroy,
    .handle_event = sys_control_widget_handle_event,
    // .show 和 .hide 可以使用默认实现或自定义
};


lv_img_dsc_t * ui_sys_control_get_res(sys_control_ui_t * sys_control_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_sys_control_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_SYS_CONTROL_ITEM)
	{
		__err("error MAX_SYS_CONTROL_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_SYS_CONTROL_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_sys_control_res_bmp[icon] == NULL) || icon > (MAX_SYS_CONTROL_ITEM - 1))
		return NULL;

	if(sys_control_para->lv_sys_control_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_sys_control_res_bmp[icon], &sys_control_para->lv_sys_control_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_sys_control_res_bmp[icon], &sys_control_para->lv_sys_control_bmp[icon], &sys_control_para->lv_sys_control_icon[icon]);
	}

	return &sys_control_para->lv_sys_control_icon[icon];
}

void ui_sys_control_uninit_res(sys_control_ui_t * sys_control_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_SYS_CONTROL_ITEM; i++ )
	{
		if( sys_control_para->lv_sys_control_bmp[i] )
		{
			dsk_theme_close( sys_control_para->lv_sys_control_bmp[i] );
			sys_control_para->lv_sys_control_bmp[i] = NULL;
		}
	}
}


// --- 公共函数 ---
/**
 * @brief 向UI管理器注册Radio控件
 */
void ui_sys_control_register_init(void) {
    ui_manager_register(MANGER_ID_SYSTEM_CONTROLS, &sys_control_ops);
}


// --- 静态函数实现 ---

/**
 * @brief 创建Radio主界面的所有LVGL对象
 */
static lv_obj_t* sys_control_widget_create(lv_obj_t* parent, void* arg) {
    if (!parent) return NULL;

    // 将通用参数指针转换为Radio专用的载荷指针
    // UI管理器应确保在调用create时传递了正确的初始化数据

    // 1. 分配并关联私有数据结构
    	memset(&g_sys_control_state, 0, sizeof(g_sys_control_state));
    g_sys_control_state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_sys_control_state.container);
    lv_obj_set_size(g_sys_control_state.container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_sys_control_state.container, LV_OBJ_FLAG_SCROLLABLE);

	
	g_sys_control_state.ui_system_control_pancel = lv_obj_create(g_sys_control_state.container);
	lv_obj_set_width(g_sys_control_state.ui_system_control_pancel, 390);
	lv_obj_set_height(g_sys_control_state.ui_system_control_pancel, 348);
	lv_obj_set_x(g_sys_control_state.ui_system_control_pancel, 0);
	lv_obj_set_y(g_sys_control_state.ui_system_control_pancel, 0);
	lv_obj_clear_flag(g_sys_control_state.ui_system_control_pancel, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_system_control_pancel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_system_control_pancel, lv_color_hex(0x666666), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_system_control_pancel, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_system_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_system_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_system_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_system_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_system_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_volume_control_pancel = lv_obj_create(g_sys_control_state.ui_system_control_pancel);
	lv_obj_set_height(g_sys_control_state.ui_volume_control_pancel, 84);
	lv_obj_set_width(g_sys_control_state.ui_volume_control_pancel, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_volume_control_pancel, 0);
	lv_obj_set_y(g_sys_control_state.ui_volume_control_pancel, 17);
	lv_obj_set_flex_flow(g_sys_control_state.ui_volume_control_pancel, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(g_sys_control_state.ui_volume_control_pancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(g_sys_control_state.ui_volume_control_pancel, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_volume_control_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_volume_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_volum_pancel = lv_obj_create(g_sys_control_state.ui_volume_control_pancel);
	lv_obj_set_width(g_sys_control_state.ui_volum_pancel, 80);
	lv_obj_set_height(g_sys_control_state.ui_volum_pancel, 72);
	lv_obj_set_flex_flow(g_sys_control_state.ui_volum_pancel, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(g_sys_control_state.ui_volum_pancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(g_sys_control_state.ui_volum_pancel, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_volum_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_volum_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_volume_icon = lv_img_create(g_sys_control_state.ui_volum_pancel);
	lv_img_set_src(g_sys_control_state.ui_volume_icon, ui_sys_control_get_res(&g_sys_control_state,VOLUME_SMALL_BMP));
	lv_obj_set_width(g_sys_control_state.ui_volume_icon, 38);
	lv_obj_set_height(g_sys_control_state.ui_volume_icon, 30);
	lv_obj_set_x(g_sys_control_state.ui_volume_icon, 30);
	lv_obj_set_y(g_sys_control_state.ui_volume_icon, 6);
	lv_obj_add_flag(g_sys_control_state.ui_volume_icon, LV_OBJ_FLAG_ADV_HITTEST);	  /// Flags
	lv_obj_clear_flag(g_sys_control_state.ui_volume_icon, LV_OBJ_FLAG_SCROLLABLE);		/// Flags

	g_sys_control_state.ui_volume_val = lv_label_create(g_sys_control_state.ui_volum_pancel);
	lv_obj_set_width(g_sys_control_state.ui_volume_val, LV_SIZE_CONTENT);	/// 1
	lv_obj_set_height(g_sys_control_state.ui_volume_val, LV_SIZE_CONTENT);	  /// 1
	lv_label_set_text(g_sys_control_state.ui_volume_val, "20");
	lv_obj_set_style_text_color(g_sys_control_state.ui_volume_val, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_sys_control_state.ui_volume_val, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(g_sys_control_state.ui_volume_val, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_volume_slider = lv_slider_create(g_sys_control_state.ui_volume_control_pancel);
	lv_slider_set_value(g_sys_control_state.ui_volume_slider, 50, LV_ANIM_OFF);
	lv_slider_set_left_value(g_sys_control_state.ui_volume_slider, 0, LV_ANIM_OFF);
	lv_obj_set_width(g_sys_control_state.ui_volume_slider, 260);
	lv_obj_set_height(g_sys_control_state.ui_volume_slider, 84);
	lv_obj_set_x(g_sys_control_state.ui_volume_slider, 94);
	lv_obj_set_y(g_sys_control_state.ui_volume_slider, 0);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_volume_slider, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_volume_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_set_style_radius(g_sys_control_state.ui_volume_slider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_volume_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_volume_slider, 128, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_set_style_bg_color(g_sys_control_state.ui_volume_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_volume_slider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

	g_sys_control_state.ui_backlight_control_pancel = lv_obj_create(g_sys_control_state.ui_system_control_pancel);
	lv_obj_set_height(g_sys_control_state.ui_backlight_control_pancel, 84);
	lv_obj_set_width(g_sys_control_state.ui_backlight_control_pancel, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_backlight_control_pancel, 0);
	lv_obj_set_y(g_sys_control_state.ui_backlight_control_pancel, 128);
	lv_obj_set_flex_flow(g_sys_control_state.ui_backlight_control_pancel, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(g_sys_control_state.ui_backlight_control_pancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(g_sys_control_state.ui_backlight_control_pancel, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_backlight_control_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_backlight_control_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_backlight_pancel = lv_obj_create(g_sys_control_state.ui_backlight_control_pancel);
	lv_obj_set_width(g_sys_control_state.ui_backlight_pancel, 80);
	lv_obj_set_height(g_sys_control_state.ui_backlight_pancel, 72);
	lv_obj_set_flex_flow(g_sys_control_state.ui_backlight_pancel, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(g_sys_control_state.ui_backlight_pancel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(g_sys_control_state.ui_backlight_pancel, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_backlight_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_backlight_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_backlight_icon = lv_img_create(g_sys_control_state.ui_backlight_pancel);
	lv_img_set_src(g_sys_control_state.ui_backlight_icon, ui_sys_control_get_res(&g_sys_control_state,BACKLIGHT_ICON_BMP));
	lv_obj_set_height(g_sys_control_state.ui_backlight_icon, 38);
	lv_obj_set_width(g_sys_control_state.ui_backlight_icon, LV_SIZE_CONTENT);	/// 1
	lv_obj_set_x(g_sys_control_state.ui_backlight_icon, 30);
	lv_obj_set_y(g_sys_control_state.ui_backlight_icon, 6);
	lv_obj_add_flag(g_sys_control_state.ui_backlight_icon, LV_OBJ_FLAG_ADV_HITTEST);	 /// Flags
	lv_obj_clear_flag(g_sys_control_state.ui_backlight_icon, LV_OBJ_FLAG_SCROLLABLE);	   /// Flags

	g_sys_control_state.ui_backlight_text = lv_label_create(g_sys_control_state.ui_backlight_pancel);
	lv_obj_set_width(g_sys_control_state.ui_backlight_text, LV_SIZE_CONTENT);	/// 1
	lv_obj_set_height(g_sys_control_state.ui_backlight_text, LV_SIZE_CONTENT);	  /// 1
	lv_label_set_text(g_sys_control_state.ui_backlight_text, "High");
	lv_obj_set_style_text_color(g_sys_control_state.ui_backlight_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_sys_control_state.ui_backlight_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(g_sys_control_state.ui_backlight_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_backlight_slider = lv_slider_create(g_sys_control_state.ui_backlight_control_pancel);
	lv_slider_set_value(g_sys_control_state.ui_backlight_slider, 50, LV_ANIM_OFF);
	lv_slider_set_left_value(g_sys_control_state.ui_backlight_slider, 0, LV_ANIM_OFF);
	lv_obj_set_width(g_sys_control_state.ui_backlight_slider, 260);
	lv_obj_set_height(g_sys_control_state.ui_backlight_slider, 84);
	lv_obj_set_x(g_sys_control_state.ui_backlight_slider, 94);
	lv_obj_set_y(g_sys_control_state.ui_backlight_slider, 0);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_backlight_slider, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_backlight_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_set_style_radius(g_sys_control_state.ui_backlight_slider, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_backlight_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_backlight_slider, 128, LV_PART_INDICATOR | LV_STATE_DEFAULT);

	lv_obj_set_style_bg_color(g_sys_control_state.ui_backlight_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_backlight_slider, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sys_control_bottom_pancel = lv_obj_create(g_sys_control_state.ui_system_control_pancel);
	lv_obj_set_height(g_sys_control_state.ui_sys_control_bottom_pancel, 100);
	lv_obj_set_width(g_sys_control_state.ui_sys_control_bottom_pancel, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_sys_control_bottom_pancel, 0);
	lv_obj_set_y(g_sys_control_state.ui_sys_control_bottom_pancel, 229);
	lv_obj_set_flex_flow(g_sys_control_state.ui_sys_control_bottom_pancel, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(g_sys_control_state.ui_sys_control_bottom_pancel, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER);
	lv_obj_clear_flag(g_sys_control_state.ui_sys_control_bottom_pancel, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sys_control_bottom_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_sys_control_bottom_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_phone_pancel = lv_obj_create(g_sys_control_state.ui_sys_control_bottom_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_phone_pancel, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_phone_pancel, 100);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_phone_pancel, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_phone_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_sel_phone_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_phone_bg = lv_obj_create(g_sys_control_state.ui_sel_phone_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_phone_bg, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_phone_bg, 80);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_phone_bg, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_phone_bg, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_phone_bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_phone_bg, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_phone_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_phone_button = lv_imgbtn_create(g_sys_control_state.ui_sel_phone_bg);
	lv_imgbtn_set_src(g_sys_control_state.ui_phone_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_sys_control_get_res(&g_sys_control_state,PHONE_BUTTON_N_BMP), NULL);
	lv_imgbtn_set_src(g_sys_control_state.ui_phone_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_sys_control_get_res(&g_sys_control_state,PHONE_BUTTON_N_BMP), NULL);
	lv_obj_set_height(g_sys_control_state.ui_phone_button, 51);
	lv_obj_set_width(g_sys_control_state.ui_phone_button, LV_SIZE_CONTENT);   /// 46
	lv_obj_set_align(g_sys_control_state.ui_phone_button, LV_ALIGN_CENTER);

	g_sys_control_state.ui_phone_text = lv_label_create(g_sys_control_state.ui_sel_phone_pancel);
	lv_obj_set_height(g_sys_control_state.ui_phone_text, 19);
	lv_obj_set_width(g_sys_control_state.ui_phone_text, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_phone_text, 0);
	lv_obj_set_y(g_sys_control_state.ui_phone_text, 81);
	lv_label_set_text(g_sys_control_state.ui_phone_text, "Phone");
	lv_obj_set_style_text_color(g_sys_control_state.ui_phone_text, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_sys_control_state.ui_phone_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(g_sys_control_state.ui_phone_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_audio_pancel = lv_obj_create(g_sys_control_state.ui_sys_control_bottom_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_audio_pancel, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_audio_pancel, 100);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_audio_pancel, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_audio_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_sel_audio_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_audio_bg = lv_obj_create(g_sys_control_state.ui_sel_audio_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_audio_bg, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_audio_bg, 80);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_audio_bg, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_audio_bg, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_audio_bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_audio_bg, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_audio_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_audio_button = lv_imgbtn_create(g_sys_control_state.ui_sel_audio_bg);
	lv_imgbtn_set_src(g_sys_control_state.ui_audio_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_sys_control_get_res(&g_sys_control_state,AUDIO_BUTTON_N_BMP), NULL);
	lv_imgbtn_set_src(g_sys_control_state.ui_audio_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_sys_control_get_res(&g_sys_control_state,AUDIO_BUTTON_N_BMP), NULL);
	lv_obj_set_height(g_sys_control_state.ui_audio_button, 47);
	lv_obj_set_width(g_sys_control_state.ui_audio_button, LV_SIZE_CONTENT);   /// 53
	lv_obj_set_align(g_sys_control_state.ui_audio_button, LV_ALIGN_CENTER);

	g_sys_control_state.ui_audio_text = lv_label_create(g_sys_control_state.ui_sel_audio_pancel);
	lv_obj_set_height(g_sys_control_state.ui_audio_text, 19);
	lv_obj_set_width(g_sys_control_state.ui_audio_text, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_audio_text, 0);
	lv_obj_set_y(g_sys_control_state.ui_audio_text, 81);
	lv_label_set_text(g_sys_control_state.ui_audio_text, "Audio");
	lv_obj_set_style_text_color(g_sys_control_state.ui_audio_text, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_sys_control_state.ui_audio_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(g_sys_control_state.ui_audio_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_can_pancel = lv_obj_create(g_sys_control_state.ui_sys_control_bottom_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_can_pancel, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_can_pancel, 100);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_can_pancel, LV_OBJ_FLAG_SCROLLABLE);	   /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_can_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_sys_control_state.ui_sel_can_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_sel_can_bg = lv_obj_create(g_sys_control_state.ui_sel_can_pancel);
	lv_obj_set_width(g_sys_control_state.ui_sel_can_bg, 80);
	lv_obj_set_height(g_sys_control_state.ui_sel_can_bg, 80);
	lv_obj_clear_flag(g_sys_control_state.ui_sel_can_bg, LV_OBJ_FLAG_SCROLLABLE);	   /// Flags
	lv_obj_set_style_radius(g_sys_control_state.ui_sel_can_bg, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_sys_control_state.ui_sel_can_bg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_sys_control_state.ui_sel_can_bg, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_sys_control_state.ui_sel_can_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	g_sys_control_state.ui_can_button = lv_imgbtn_create(g_sys_control_state.ui_sel_can_bg);
	lv_imgbtn_set_src(g_sys_control_state.ui_can_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_sys_control_get_res(&g_sys_control_state,AUDIO_CAN_N_BMP), NULL);
	lv_imgbtn_set_src(g_sys_control_state.ui_can_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_sys_control_get_res(&g_sys_control_state,AUDIO_CAN_N_BMP), NULL);
	lv_obj_set_height(g_sys_control_state.ui_can_button, 40);
	lv_obj_set_width(g_sys_control_state.ui_can_button, LV_SIZE_CONTENT);	/// 61
	lv_obj_set_align(g_sys_control_state.ui_can_button, LV_ALIGN_CENTER);

	g_sys_control_state.ui_can_text = lv_label_create(g_sys_control_state.ui_sel_can_pancel);
	lv_obj_set_height(g_sys_control_state.ui_can_text, 19);
	lv_obj_set_width(g_sys_control_state.ui_can_text, lv_pct(100));
	lv_obj_set_x(g_sys_control_state.ui_can_text, 0);
	lv_obj_set_y(g_sys_control_state.ui_can_text, 81);
	lv_label_set_text(g_sys_control_state.ui_can_text, "CAN");
	lv_obj_set_style_text_color(g_sys_control_state.ui_can_text, lv_color_hex(0xD0D0D0), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_sys_control_state.ui_can_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(g_sys_control_state.ui_can_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

   

    return g_sys_control_state.container;
}

/**
 * @brief 销毁Radio控件时释放私有数据
 */
static void sys_control_widget_destroy(ui_manger_t* widget) {
     if (widget && widget->instance) {
        lv_obj_del(widget->instance);
    }
	 ui_sys_control_uninit_res(&g_sys_control_state);
    memset(&g_sys_control_state, 0, sizeof(g_sys_control_state));
}

/**
 * @brief 处理来自服务端的事件，并更新UI
 */
static int sys_control_widget_handle_event(ui_manger_t* widget, const app_message_t* msg) {
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



