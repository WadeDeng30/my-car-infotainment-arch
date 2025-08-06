#include "ui_manger.h"
#include "ui_quick_access.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>


typedef struct {
	
	lv_obj_t* container;			// 控件的主容器
	lv_obj_t * ui_quick_access_pancel;
	lv_obj_t * ui_set_wallpap_button;
	lv_obj_t * ui_wallpap_add_icon;
	lv_obj_t * ui_set_wallpap_text;
	lv_obj_t * ui_customize_mirrorapp_button;
	lv_obj_t * ui_mirror_small_icon;
	lv_obj_t * ui_customize_wallpap_text;
	lv_obj_t * ui_phone_topbar;

	HTHEME lv_quick_access_bmp[MAX_QUICK_ACCESS_ITEM];
	lv_img_dsc_t lv_quick_access_icon[MAX_QUICK_ACCESS_ITEM];
} quick_access_ui_t;


static quick_access_ui_t g_quick_access_state;

static lv_obj_t* quick_access_widget_create(lv_obj_t* parent, void* arg);
static void quick_access_widget_destroy(ui_manger_t* widget);
static int quick_access_widget_handle_event(ui_manger_t* widget, const app_message_t* msg);


// --- 全局变量 ---
// 定义这个控件的操作函数集
static const ui_manger_ops_t quick_access_ops = {
    .create = quick_access_widget_create,
    .destroy = quick_access_widget_destroy,
    .handle_event = quick_access_widget_handle_event,
    // .show 和 .hide 可以使用默认实现或自定义
};


lv_img_dsc_t * ui_quick_access_get_res(quick_access_ui_t * quick_access_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_quick_access_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_QUICK_ACCESS_ITEM)
	{
		__err("error MAX_BT_BMP_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_QUICK_ACCESS_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_quick_access_res_bmp[icon] == NULL) || icon > (MAX_QUICK_ACCESS_ITEM - 1))
		return NULL;

	if(quick_access_para->lv_quick_access_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_quick_access_res_bmp[icon], &quick_access_para->lv_quick_access_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_quick_access_res_bmp[icon], &quick_access_para->lv_quick_access_bmp[icon], &quick_access_para->lv_quick_access_icon[icon]);
	}

	return &quick_access_para->lv_quick_access_icon[icon];
}

void ui_quick_access_uninit_res(quick_access_ui_t * quick_access_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_QUICK_ACCESS_ITEM; i++ )
	{
		if( quick_access_para->lv_quick_access_bmp[i] )
		{
			dsk_theme_close( quick_access_para->lv_quick_access_bmp[i] );
			quick_access_para->lv_quick_access_bmp[i] = NULL;
		}
	}
}


// --- 公共函数 ---
/**
 * @brief 向UI管理器注册Radio控件
 */
void ui_quick_access_register_init(void) {
    ui_manager_register(MANGER_ID_QUICK_ACCESS, &quick_access_ops);
}


// --- 静态函数实现 ---

/**
 * @brief 创建Radio主界面的所有LVGL对象
 */
static lv_obj_t* quick_access_widget_create(lv_obj_t* parent, void* arg) {
    if (!parent) return NULL;

	memset(&g_quick_access_state, 0, sizeof(g_quick_access_state));
    g_quick_access_state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_quick_access_state.container);
    lv_obj_set_size(g_quick_access_state.container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_quick_access_state.container, LV_OBJ_FLAG_SCROLLABLE);

    g_quick_access_state.ui_quick_access_pancel = lv_obj_create(g_quick_access_state.container);
    lv_obj_set_width( g_quick_access_state.ui_quick_access_pancel, 340);
    lv_obj_set_height( g_quick_access_state.ui_quick_access_pancel, 680);
    lv_obj_set_x( g_quick_access_state.ui_quick_access_pancel, 0);
    lv_obj_set_y( g_quick_access_state.ui_quick_access_pancel, 0);
    lv_obj_clear_flag( g_quick_access_state.ui_quick_access_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius( g_quick_access_state.ui_quick_access_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color( g_quick_access_state.ui_quick_access_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa( g_quick_access_state.ui_quick_access_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src( g_quick_access_state.ui_quick_access_pancel, ui_quick_access_get_res(&g_quick_access_state, QUICK_ACCESS_BG_BMP), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width( g_quick_access_state.ui_quick_access_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

     g_quick_access_state.ui_set_wallpap_button = lv_btn_create( g_quick_access_state.ui_quick_access_pancel);
    lv_obj_set_width( g_quick_access_state.ui_set_wallpap_button, 270);
    lv_obj_set_height( g_quick_access_state.ui_set_wallpap_button, 68);
    lv_obj_set_x( g_quick_access_state.ui_set_wallpap_button, 15);
    lv_obj_set_y( g_quick_access_state.ui_set_wallpap_button, 477);
    lv_obj_add_flag( g_quick_access_state.ui_set_wallpap_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag( g_quick_access_state.ui_set_wallpap_button, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius( g_quick_access_state.ui_set_wallpap_button, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color( g_quick_access_state.ui_set_wallpap_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa( g_quick_access_state.ui_set_wallpap_button, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color( g_quick_access_state.ui_set_wallpap_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

     g_quick_access_state.ui_wallpap_add_icon = lv_img_create( g_quick_access_state.ui_set_wallpap_button);
    lv_img_set_src( g_quick_access_state.ui_wallpap_add_icon, ui_quick_access_get_res(&g_quick_access_state, WALLPAPER_ADD_BMP));
    lv_obj_set_width( g_quick_access_state.ui_wallpap_add_icon, 22);
    lv_obj_set_height( g_quick_access_state.ui_wallpap_add_icon, 20);
    lv_obj_set_x( g_quick_access_state.ui_wallpap_add_icon, 10);
    lv_obj_set_y( g_quick_access_state.ui_wallpap_add_icon, 15);
    lv_obj_add_flag( g_quick_access_state.ui_wallpap_add_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag( g_quick_access_state.ui_wallpap_add_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

     g_quick_access_state.ui_set_wallpap_text = lv_label_create( g_quick_access_state.ui_set_wallpap_button);
    lv_obj_set_width( g_quick_access_state.ui_set_wallpap_text, 200);
    lv_obj_set_height( g_quick_access_state.ui_set_wallpap_text, 30);
    lv_obj_set_x( g_quick_access_state.ui_set_wallpap_text, 42);
    lv_obj_set_y( g_quick_access_state.ui_set_wallpap_text, 16);
    lv_label_set_text( g_quick_access_state.ui_set_wallpap_text, "Set as wallpaper");
    lv_obj_set_style_text_color( g_quick_access_state.ui_set_wallpap_text, lv_color_hex(0x211D1D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa( g_quick_access_state.ui_set_wallpap_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align( g_quick_access_state.ui_set_wallpap_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font( g_quick_access_state.ui_set_wallpap_text, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

     g_quick_access_state.ui_customize_mirrorapp_button = lv_btn_create( g_quick_access_state.ui_quick_access_pancel);
    lv_obj_set_width( g_quick_access_state.ui_customize_mirrorapp_button, 270);
    lv_obj_set_height(g_quick_access_state.ui_customize_mirrorapp_button, 68);
    lv_obj_set_x( g_quick_access_state.ui_customize_mirrorapp_button, 13);
    lv_obj_set_y( g_quick_access_state.ui_customize_mirrorapp_button, 556);
    lv_obj_add_flag( g_quick_access_state.ui_customize_mirrorapp_button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag( g_quick_access_state.ui_customize_mirrorapp_button, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius( g_quick_access_state.ui_customize_mirrorapp_button, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color( g_quick_access_state.ui_customize_mirrorapp_button, lv_color_hex(0x000B0C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa( g_quick_access_state.ui_customize_mirrorapp_button, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color( g_quick_access_state.ui_customize_mirrorapp_button, lv_color_hex(0x00FFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa( g_quick_access_state.ui_customize_mirrorapp_button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width( g_quick_access_state.ui_customize_mirrorapp_button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

     g_quick_access_state.ui_mirror_small_icon = lv_img_create( g_quick_access_state.ui_customize_mirrorapp_button);
    lv_img_set_src( g_quick_access_state.ui_mirror_small_icon, ui_quick_access_get_res(&g_quick_access_state, MIRROR_SMALL_ICON_BMP));
    lv_obj_set_width( g_quick_access_state.ui_mirror_small_icon, 50);
    lv_obj_set_height( g_quick_access_state.ui_mirror_small_icon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x( g_quick_access_state.ui_mirror_small_icon, 4);
    lv_obj_set_y( g_quick_access_state.ui_mirror_small_icon, -4);
    lv_obj_add_flag( g_quick_access_state.ui_mirror_small_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag( g_quick_access_state.ui_mirror_small_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

     g_quick_access_state.ui_customize_wallpap_text = lv_label_create( g_quick_access_state.ui_customize_mirrorapp_button);
    lv_obj_set_width( g_quick_access_state.ui_customize_wallpap_text, 176);
    lv_obj_set_height( g_quick_access_state.ui_customize_wallpap_text, 38);
    lv_obj_set_x( g_quick_access_state.ui_customize_wallpap_text, 51);
    lv_obj_set_y( g_quick_access_state.ui_customize_wallpap_text, 0);
    lv_obj_set_align( g_quick_access_state.ui_customize_wallpap_text, LV_ALIGN_LEFT_MID);
    lv_label_set_text( g_quick_access_state.ui_customize_wallpap_text, "Customize Wallpaper in MirrorAPP+");
    lv_obj_set_style_text_align( g_quick_access_state.ui_customize_wallpap_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font( g_quick_access_state.ui_customize_wallpap_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);

     g_quick_access_state.ui_phone_topbar = lv_obj_create( g_quick_access_state.ui_quick_access_pancel);
    lv_obj_set_width( g_quick_access_state.ui_phone_topbar, 80);
    lv_obj_set_height( g_quick_access_state.ui_phone_topbar, 8);
    lv_obj_set_x( g_quick_access_state.ui_phone_topbar, 105);
    lv_obj_set_y( g_quick_access_state.ui_phone_topbar, 5);
    lv_obj_clear_flag(g_quick_access_state.ui_phone_topbar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius( g_quick_access_state.ui_phone_topbar, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color( g_quick_access_state.ui_phone_topbar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa( g_quick_access_state.ui_phone_topbar, 127, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color( g_quick_access_state.ui_phone_topbar, lv_color_hex(0x00FFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa( g_quick_access_state.ui_phone_topbar, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
   

    return g_quick_access_state.container;
}

/**
 * @brief 销毁Radio控件时释放私有数据
 */
static void quick_access_widget_destroy(ui_manger_t* widget) {
     if (widget && widget->instance) {
        lv_obj_del(widget->instance);
    }
	 ui_quick_access_uninit_res(&g_quick_access_state);
    memset(&g_quick_access_state, 0, sizeof(g_quick_access_state));
}

/**
 * @brief 处理来自服务端的事件，并更新UI
 */
static int quick_access_widget_handle_event(ui_manger_t* widget, const app_message_t* msg) {
    if (!widget || !widget->instance || !msg) return -1;


    // 将通用载荷转换为Radio专用载荷
    //const msg_payload_service_radio_t* payload = (const msg_payload_service_radio_t*)msg->payload;

    switch (msg->command) {

        default:
            // 其他不关心的消息，直接忽略
            break;
    }
    return 0;
}




