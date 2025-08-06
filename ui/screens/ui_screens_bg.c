#include "ui_screens_bg.h"


typedef struct {
	lv_obj_t* ui_background;			// 控件的主容器
	lv_obj_t* ui_bg_block1;			// 控件的主容器
	lv_obj_t* ui_bg_block_pic;			// 控件的主容器
	lv_obj_t* ui_bg_block2;			// 控件的主容器
	lv_obj_t* ui_bg_block2_pic;			// 控件的主容器
	
	HTHEME lv_background_bmp[MAX_BACKGROUND_BMP_ITEM];
	lv_img_dsc_t lv_background_icon[MAX_BACKGROUND_BMP_ITEM];
} background_ui_t;

static background_ui_t g_background_state;



lv_img_dsc_t * ui_background_get_res(background_ui_t * background_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_background_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_BACKGROUND_BMP_ITEM)
	{
		__err("error MAX_SYS_CONTROL_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_BACKGROUND_BMP_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_background_res_bmp[icon] == NULL) || icon > (MAX_BACKGROUND_BMP_ITEM - 1))
		return NULL;

	if(background_para->lv_background_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_background_res_bmp[icon], &background_para->lv_background_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_background_res_bmp[icon], &background_para->lv_background_bmp[icon], &background_para->lv_background_icon[icon]);
	}

	return &background_para->lv_background_icon[icon];
}

void ui_background_uninit_res(background_ui_t * background_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_BACKGROUND_BMP_ITEM; i++ )
	{
		if( background_para->lv_background_bmp[i] )
		{
			dsk_theme_close( background_para->lv_background_bmp[i] );
			background_para->lv_background_bmp[i] = NULL;
		}
	}
}




void screen_create_background(lv_obj_t* screen, ui_screen_id_t screen_id) {
    if (!screen)
		return;
	
	memset(&g_background_state, 0, sizeof(g_background_state));
	    g_background_state.ui_background = lv_obj_create(screen);
    lv_obj_set_width(g_background_state.ui_background, 1280);
    lv_obj_set_height(g_background_state.ui_background, 720);
    lv_obj_clear_flag(g_background_state.ui_background, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
      lv_obj_set_style_radius(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_background_state.ui_background, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_background_state.ui_background, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_background_state.ui_bg_block1 = lv_obj_create(g_background_state.ui_background);
    lv_obj_set_width(g_background_state.ui_bg_block1, 1280);
    lv_obj_set_height(g_background_state.ui_bg_block1, 720);
    lv_obj_clear_flag(g_background_state.ui_bg_block1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
     lv_obj_set_style_radius(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_background_state.ui_bg_block1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_background_state.ui_bg_block1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_background_state.ui_bg_block_pic = lv_img_create(g_background_state.ui_bg_block1);
    lv_img_set_src(g_background_state.ui_bg_block_pic, ui_background_get_res(&g_background_state,BACKGROUND_BMP));
    lv_obj_set_width(g_background_state.ui_bg_block_pic, lv_pct(100));
    lv_obj_set_height(g_background_state.ui_bg_block_pic, lv_pct(100));
    lv_obj_add_flag(g_background_state.ui_bg_block_pic, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_background_state.ui_bg_block_pic, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    g_background_state.ui_bg_block2 = lv_obj_create(g_background_state.ui_background);
    lv_obj_set_width(g_background_state.ui_bg_block2, lv_pct(100));
    lv_obj_set_height(g_background_state.ui_bg_block2, lv_pct(100));
    lv_obj_add_flag(g_background_state.ui_bg_block2, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(g_background_state.ui_bg_block2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
     lv_obj_set_style_radius(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_background_state.ui_bg_block2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_background_state.ui_bg_block2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_background_state.ui_bg_block2_pic = lv_img_create(g_background_state.ui_bg_block2);
    lv_img_set_src(g_background_state.ui_bg_block2_pic, ui_background_get_res(&g_background_state,BACKGROUND_BMP));
    lv_obj_set_width(g_background_state.ui_bg_block2_pic, lv_pct(100));
    lv_obj_set_height(g_background_state.ui_bg_block2_pic, lv_pct(100));
    lv_obj_add_flag(g_background_state.ui_bg_block2_pic, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_background_state.ui_bg_block2_pic, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
}



