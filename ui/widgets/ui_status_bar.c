#include "ui_manger.h"
#include "ui_status_bar.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>


typedef struct {
    	lv_obj_t* container;            // 控件的主容器
    	lv_obj_t * ui_status_bar;
	lv_obj_t * ui_status_top_board;
	lv_obj_t * ui_timetext;
	lv_obj_t * ui_status_icon_bg;
	lv_obj_t * ui_bt1_icon;
	lv_obj_t * ui_bt2_icon;
	lv_obj_t * ui_wifi_icon;
	lv_obj_t * ui_mute_icon;
	lv_obj_t * ui_more_than_button;
	lv_obj_t * ui_ellipsis_text;
	lv_obj_t * ui_control_status_panel;
	lv_obj_t * ui_volume_button;
	lv_obj_t * ui_mix_button;
	lv_obj_t * ui_dev_list_button;
	lv_obj_t * ui_setting_button;
	lv_obj_t * ui_mrrror_switch_button;
	lv_obj_t * ui_home_button;
	lv_obj_t * ui_quick_access_pancel;
	lv_obj_t * ui_set_wallpap_button;
	lv_obj_t * ui_wallpap_add_icon;
	lv_obj_t * ui_set_wallpap_text;
	lv_obj_t * ui_customize_mirrorapp_button;
	lv_obj_t * ui_mirror_small_icon;
	lv_obj_t * ui_customize_wallpap_text;
	lv_obj_t * ui_phone_topbar;

	HTHEME lv_status_bar_bmp[MAX_STATUS_BAR_ITEM];
	lv_img_dsc_t lv_status_bar_icon[MAX_STATUS_BAR_ITEM];
} status_bar_ui_t;

static status_bar_ui_t g_status_bar_state;

static lv_obj_t* status_bar_widget_create(lv_obj_t* parent, void* arg);
static void status_bar_widget_destroy(ui_manger_t* widget);
static int status_bar_widget_handle_event(ui_manger_t* widget, const app_message_t* msg);



// --- 全局变量 ---
// 定义这个控件的操作函数集
static const ui_manger_ops_t status_bar_ops = {
    .create = status_bar_widget_create,
    .destroy = status_bar_widget_destroy,
    .handle_event = status_bar_widget_handle_event,
    // .show 和 .hide 可以使用默认实现或自定义
};

lv_img_dsc_t * ui_status_bar_get_res(status_bar_ui_t * status_bar_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_status_bar_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_STATUS_BAR_ITEM)
	{
		__err("error MAX_BT_BMP_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_STATUS_BAR_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_status_bar_res_bmp[icon] == NULL) || icon > (MAX_STATUS_BAR_ITEM - 1))
		return NULL;

	if(status_bar_para->lv_status_bar_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_status_bar_res_bmp[icon], &status_bar_para->lv_status_bar_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_status_bar_res_bmp[icon], &status_bar_para->lv_status_bar_bmp[icon], &status_bar_para->lv_status_bar_icon[icon]);
	}

	return &status_bar_para->lv_status_bar_icon[icon];
}

void ui_status_bar_uninit_res(status_bar_ui_t * status_bar_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_STATUS_BAR_ITEM; i++ )
	{
		if( status_bar_para->lv_status_bar_bmp[i] )
		{
			dsk_theme_close( status_bar_para->lv_status_bar_bmp[i] );
			status_bar_para->lv_status_bar_bmp[i] = NULL;
		}
	}
}




// --- 公共函数 ---
/**
 * @brief 向UI管理器注册Radio控件
 */
void ui_status_bar_register_init(void) {
    ui_manager_register(MANGER_ID_SIDEBAR_MENU, &status_bar_ops);
}


// --- 静态函数实现 ---

/**
 * @brief 创建Radio主界面的所有LVGL对象
 */
static lv_obj_t* status_bar_widget_create(lv_obj_t* parent, void* arg) {
    if (!parent) return NULL;

  	memset(&g_status_bar_state, 0, sizeof(g_status_bar_state));
    g_status_bar_state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_status_bar_state.container);
    lv_obj_set_size(g_status_bar_state.container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_status_bar_state.container, LV_OBJ_FLAG_SCROLLABLE);

 g_status_bar_state.ui_status_bar = lv_obj_create(g_status_bar_state.container);    
 lv_obj_set_width(g_status_bar_state.ui_status_bar, 80);    
 lv_obj_set_height(g_status_bar_state.ui_status_bar, 720);    
 lv_obj_clear_flag(g_status_bar_state.ui_status_bar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags    
 lv_obj_set_style_radius(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_bg_color(g_status_bar_state.ui_status_bar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_bg_opa(g_status_bar_state.ui_status_bar, 230, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_border_width(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_left(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_right(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_top(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_bottom(g_status_bar_state.ui_status_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);  
 
 g_status_bar_state.ui_status_top_board = lv_obj_create(g_status_bar_state.ui_status_bar);    
 lv_obj_set_width(g_status_bar_state.ui_status_top_board, 79);    
 lv_obj_set_height(g_status_bar_state.ui_status_top_board, 140);   
 lv_obj_set_x(g_status_bar_state.ui_status_top_board, 0);    
 lv_obj_set_y(g_status_bar_state.ui_status_top_board, 20);   
 lv_obj_clear_flag(g_status_bar_state.ui_status_top_board, LV_OBJ_FLAG_SCROLLABLE);      /// Flags    
 lv_obj_set_style_bg_color(g_status_bar_state.ui_status_top_board, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_bg_opa(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_border_width(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_left(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_right(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_top(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_bottom(g_status_bar_state.ui_status_top_board, 0, LV_PART_MAIN | LV_STATE_DEFAULT); 
 
 g_status_bar_state.ui_timetext = lv_label_create(g_status_bar_state.ui_status_top_board);    
 lv_obj_set_width(g_status_bar_state.ui_timetext, 61);    
 lv_obj_set_height(g_status_bar_state.ui_timetext, 22);    
 lv_obj_set_x(g_status_bar_state.ui_timetext, 7);    
 lv_obj_set_y(g_status_bar_state.ui_timetext, 3);    
 lv_label_set_long_mode(g_status_bar_state.ui_timetext, LV_LABEL_LONG_CLIP);   
 lv_label_set_text(g_status_bar_state.ui_timetext, "12:34");    
 lv_obj_set_style_text_color(g_status_bar_state.ui_timetext, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_text_opa(g_status_bar_state.ui_timetext, 255, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_text_align(g_status_bar_state.ui_timetext, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_text_font(g_status_bar_state.ui_timetext, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);    
 
 g_status_bar_state.ui_status_icon_bg = lv_obj_create(g_status_bar_state.ui_status_top_board);    
 lv_obj_set_width(g_status_bar_state.ui_status_icon_bg, 79);    
 lv_obj_set_height(g_status_bar_state.ui_status_icon_bg, 110);    
 lv_obj_set_x(g_status_bar_state.ui_status_icon_bg, 0);    
 lv_obj_set_y(g_status_bar_state.ui_status_icon_bg, 26);    
 lv_obj_set_flex_flow(g_status_bar_state.ui_status_icon_bg, LV_FLEX_FLOW_ROW_WRAP);    
 lv_obj_set_flex_align(g_status_bar_state.ui_status_icon_bg, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);    
 lv_obj_clear_flag(g_status_bar_state.ui_status_icon_bg, LV_OBJ_FLAG_SCROLLABLE);      /// Flags    
 lv_obj_set_style_bg_color(g_status_bar_state.ui_status_icon_bg, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_bg_opa(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_border_width(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_left(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_right(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_top(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_bottom(g_status_bar_state.ui_status_icon_bg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 
 g_status_bar_state.ui_bt1_icon = lv_img_create(g_status_bar_state.ui_status_icon_bg);    
 lv_img_set_src(g_status_bar_state.ui_bt1_icon, ui_status_bar_get_res(&g_status_bar_state, BT1_ICON_BMP));    
 lv_obj_set_width(g_status_bar_state.ui_bt1_icon, 28);    
 lv_obj_set_height(g_status_bar_state.ui_bt1_icon, 30);    
 lv_obj_set_x(g_status_bar_state.ui_bt1_icon, 9);    
 lv_obj_set_y(g_status_bar_state.ui_bt1_icon, 15);    
 lv_obj_add_flag(g_status_bar_state.ui_bt1_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags    
 lv_obj_clear_flag(g_status_bar_state.ui_bt1_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags    
 
 g_status_bar_state.ui_bt2_icon = lv_img_create(g_status_bar_state.ui_status_icon_bg);    
 lv_img_set_src(g_status_bar_state.ui_bt2_icon, ui_status_bar_get_res(&g_status_bar_state, BT2_ICON_BMP));   
 lv_obj_set_width(g_status_bar_state.ui_bt2_icon, 33);    
 lv_obj_set_height(g_status_bar_state.ui_bt2_icon, 30);    
 lv_obj_set_x(g_status_bar_state.ui_bt2_icon, 42);    
 lv_obj_set_y(g_status_bar_state.ui_bt2_icon, 15);    
 lv_obj_add_flag(g_status_bar_state.ui_bt2_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags    
 lv_obj_clear_flag(g_status_bar_state.ui_bt2_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags   
 
 g_status_bar_state.ui_wifi_icon = lv_img_create(g_status_bar_state.ui_status_icon_bg);    
 lv_img_set_src(g_status_bar_state.ui_wifi_icon, ui_status_bar_get_res(&g_status_bar_state, WIFI_ICON_BMP));   
 lv_obj_set_width(g_status_bar_state.ui_wifi_icon, 26);    
 lv_obj_set_height(g_status_bar_state.ui_wifi_icon, 22);   
 lv_obj_set_x(g_status_bar_state.ui_wifi_icon, 9);   
 lv_obj_set_y(g_status_bar_state.ui_wifi_icon, 52);   
 lv_obj_add_flag(g_status_bar_state.ui_wifi_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags   
 lv_obj_clear_flag(g_status_bar_state.ui_wifi_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags 
 
 g_status_bar_state.ui_mute_icon = lv_img_create(g_status_bar_state.ui_status_icon_bg);   
 lv_img_set_src(g_status_bar_state.ui_mute_icon, ui_status_bar_get_res(&g_status_bar_state, MUTE_STATUS_ICON_BMP));  
 lv_obj_set_width(g_status_bar_state.ui_mute_icon, 32);    
 lv_obj_set_height(g_status_bar_state.ui_mute_icon, 27);    
 lv_obj_set_x(g_status_bar_state.ui_mute_icon, 42);    
 lv_obj_set_y(g_status_bar_state.ui_mute_icon, 51);   
 lv_obj_add_flag(g_status_bar_state.ui_mute_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags    
 lv_obj_clear_flag(g_status_bar_state.ui_mute_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags   
 
 g_status_bar_state.ui_more_than_button = lv_btn_create(g_status_bar_state.ui_status_icon_bg);   
 lv_obj_set_width(g_status_bar_state.ui_more_than_button, 40);    
 lv_obj_set_height(g_status_bar_state.ui_more_than_button, 24);   
 lv_obj_set_x(g_status_bar_state.ui_more_than_button, 20);    
 lv_obj_set_y(g_status_bar_state.ui_more_than_button, 84);    
 lv_obj_add_flag(g_status_bar_state.ui_more_than_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags  
 lv_obj_clear_flag(g_status_bar_state.ui_more_than_button, LV_OBJ_FLAG_SCROLLABLE);      /// Flags   
 lv_obj_set_style_bg_color(g_status_bar_state.ui_more_than_button, lv_color_hex(0x646464), LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_bg_opa(g_status_bar_state.ui_more_than_button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_border_color(g_status_bar_state.ui_more_than_button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_border_opa(g_status_bar_state.ui_more_than_button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_border_width(g_status_bar_state.ui_more_than_button, 0, LV_PART_MAIN | LV_STATE_DEFAULT); 
 
 g_status_bar_state.ui_ellipsis_text = lv_label_create(g_status_bar_state.ui_more_than_button);   
 lv_obj_set_width(g_status_bar_state.ui_ellipsis_text, 28);   
 lv_obj_set_height(g_status_bar_state.ui_ellipsis_text, 26);  
 lv_obj_set_x(g_status_bar_state.ui_ellipsis_text, 1);    
 lv_obj_set_y(g_status_bar_state.ui_ellipsis_text, -2);   
 lv_obj_set_align(g_status_bar_state.ui_ellipsis_text, LV_ALIGN_CENTER);  
 lv_label_set_text(g_status_bar_state.ui_ellipsis_text, ". . .");   
 lv_obj_set_style_text_align(g_status_bar_state.ui_ellipsis_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_text_decor(g_status_bar_state.ui_ellipsis_text, LV_TEXT_DECOR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_text_font(g_status_bar_state.ui_ellipsis_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);   
 
 g_status_bar_state.ui_control_status_panel = lv_obj_create(g_status_bar_state.ui_status_bar);    
 lv_obj_set_height(g_status_bar_state.ui_control_status_panel, 323);    
 lv_obj_set_width(g_status_bar_state.ui_control_status_panel, lv_pct(100));   
 lv_obj_set_x(g_status_bar_state.ui_control_status_panel, 0);    
 lv_obj_set_y(g_status_bar_state.ui_control_status_panel, 210);    
 lv_obj_set_flex_flow(g_status_bar_state.ui_control_status_panel, LV_FLEX_FLOW_ROW_WRAP);   
 lv_obj_set_flex_align(g_status_bar_state.ui_control_status_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);   
 lv_obj_clear_flag(g_status_bar_state.ui_control_status_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags    
 lv_obj_set_style_radius(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_bg_color(g_status_bar_state.ui_control_status_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_bg_opa(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_border_width(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_left(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_right(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 lv_obj_set_style_pad_top(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);   
 lv_obj_set_style_pad_bottom(g_status_bar_state.ui_control_status_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);    
 
 g_status_bar_state.ui_volume_button = lv_imgbtn_create(g_status_bar_state.ui_control_status_panel);    
 lv_imgbtn_set_src(g_status_bar_state.ui_volume_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_status_bar_get_res(&g_status_bar_state, VOLUME_CONTROL_N_BMP), NULL);  
 lv_imgbtn_set_src(g_status_bar_state.ui_volume_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, VOLUME_CONTROL_S_BMP), NULL);   
 lv_obj_set_height(g_status_bar_state.ui_volume_button, 53);    
 lv_obj_set_width(g_status_bar_state.ui_volume_button, LV_SIZE_CONTENT);   /// 1    
 
 g_status_bar_state.ui_mix_button = lv_imgbtn_create(g_status_bar_state.ui_control_status_panel);   
 lv_imgbtn_set_src(g_status_bar_state.ui_mix_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_status_bar_get_res(&g_status_bar_state, MIX_BUTTON_N_BMP), NULL);  
 lv_imgbtn_set_src(g_status_bar_state.ui_mix_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, MIX_BUTTON_P_BMP), NULL);   
 lv_obj_set_height(g_status_bar_state.ui_mix_button, 55);   
 lv_obj_set_width(g_status_bar_state.ui_mix_button, LV_SIZE_CONTENT);   /// 1  
 
 g_status_bar_state.ui_dev_list_button = lv_imgbtn_create(g_status_bar_state.ui_control_status_panel);    
 lv_imgbtn_set_src(g_status_bar_state.ui_dev_list_button, LV_IMGBTN_STATE_RELEASED, NULL,ui_status_bar_get_res(&g_status_bar_state, DEV_LIST_BUTTON_BMP), NULL);  
  lv_imgbtn_set_src(g_status_bar_state.ui_dev_list_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, DEV_LIST_BUTTON_BMP), NULL); 
 lv_obj_set_height(g_status_bar_state.ui_dev_list_button, 50);    
 lv_obj_set_width(g_status_bar_state.ui_dev_list_button, LV_SIZE_CONTENT);   /// 1 
 
 g_status_bar_state.ui_setting_button = lv_imgbtn_create(g_status_bar_state.ui_control_status_panel);    
 lv_imgbtn_set_src(g_status_bar_state.ui_setting_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_status_bar_get_res(&g_status_bar_state, SETTING_ICON_BMP), NULL);  
 lv_imgbtn_set_src(g_status_bar_state.ui_setting_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, SETTING_ICON_BMP), NULL);    
 lv_obj_set_height(g_status_bar_state.ui_setting_button, 61);    
 lv_obj_set_width(g_status_bar_state.ui_setting_button, LV_SIZE_CONTENT);   /// 1    
 
 g_status_bar_state.ui_mrrror_switch_button = lv_imgbtn_create(g_status_bar_state.ui_control_status_panel);    
 lv_imgbtn_set_src(g_status_bar_state.ui_mrrror_switch_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_status_bar_get_res(&g_status_bar_state, MIRROR_SWITCH_BMP), NULL);  
 lv_imgbtn_set_src(g_status_bar_state.ui_mrrror_switch_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, MIRROR_SWITCH_BMP), NULL);    
 lv_obj_set_height(g_status_bar_state.ui_mrrror_switch_button, 43);    
 lv_obj_set_width(g_status_bar_state.ui_mrrror_switch_button, LV_SIZE_CONTENT);   /// 1 
 
 g_status_bar_state.ui_home_button = lv_imgbtn_create(g_status_bar_state.ui_status_bar);    
 lv_imgbtn_set_src(g_status_bar_state.ui_home_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_status_bar_get_res(&g_status_bar_state, HOME_BUTTON_BMP), NULL);  
 lv_imgbtn_set_src(g_status_bar_state.ui_home_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_status_bar_get_res(&g_status_bar_state, HOME_BUTTON_BMP), NULL);    
 lv_obj_set_width(g_status_bar_state.ui_home_button, 51);   
 lv_obj_set_height(g_status_bar_state.ui_home_button, 53);    
 lv_obj_set_x(g_status_bar_state.ui_home_button, 12);    
 lv_obj_set_y(g_status_bar_state.ui_home_button, 648);  
 

    return g_status_bar_state.container;
}

/**
 * @brief 销毁Radio控件时释放私有数据
 */
static void status_bar_widget_destroy(ui_manger_t* widget) {
    if (widget && widget->instance) {
        lv_obj_del(widget->instance);
    }
	ui_status_bar_uninit_res(&g_status_bar_state);
    memset(&g_status_bar_state, 0, sizeof(g_status_bar_state));
}

/**
 * @brief 处理来自服务端的事件，并更新UI
 */
static int status_bar_widget_handle_event(ui_manger_t* widget, const app_message_t* msg) {
    if (!widget || !widget->instance || !msg) return -1;



    switch (msg->command) {

        default:
            // 其他不关心的消息，直接忽略
            break;
    }
    return 0;
}



