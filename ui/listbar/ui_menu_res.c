#include "ui_menu.h"
#include "ui_menu_res.h"

///////////////////// VARIABLES ////////////////////


///////////////////// FUNCTIONS ////////////////////

lv_img_dsc_t * ui_menu_get_res(ui_menu_para_t * menu_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_menu_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_MENU_BMP_ITEM)
	{
		__err("error MAX_MENU_BMP_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_MENU_BMP_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_menu_res_bmp[icon] == NULL) || icon > (MAX_MENU_BMP_ITEM - 1))
		return NULL;

	if(menu_para->ui_menu_res_para.lv_menu_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_menu_res_bmp[icon], &menu_para->ui_menu_res_para.lv_menu_bmp[icon]);
		Lzma_get_theme_buf(lv_menu_res_bmp[icon], &menu_para->ui_menu_res_para.lv_menu_bmp[icon], &menu_para->ui_menu_res_para.lv_menu_icon[icon]);
	}
	
	return &menu_para->ui_menu_res_para.lv_menu_icon[icon];
}

void ui_menu_init_res(ui_menu_para_t * menu_para)
{
	int ret;
    __u32 i;

	/*for( i = 0; i < MAX_MENU_BMP_ITEM; i++ )
	{
		if(lv_menu_res_bmp[i])
			Lzma_get_theme(lv_menu_res_bmp[i], &menu_para->ui_menu_res_para.lv_menu_bmp[i]);
	}*/
}

void ui_menu_uninit_res(ui_menu_para_t * menu_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_MENU_BMP_ITEM; i++ )
	{
		if( menu_para->ui_menu_res_para.lv_menu_bmp[i] )
		{
			dsk_theme_close( menu_para->ui_menu_res_para.lv_menu_bmp[i] );
			menu_para->ui_menu_res_para.lv_menu_bmp[i] = NULL;
		}
	}
}

