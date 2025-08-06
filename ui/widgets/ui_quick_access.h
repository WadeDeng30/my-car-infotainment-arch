#ifndef _UI_QUICK_ACCESS_H_
#define _UI_QUICK_ACCESS_H_

void ui_quick_access_register_init(void);

enum
{
	QUICK_ACCESS_BG_BMP,
	WALLPAPER_ADD_BMP,	
	MIRROR_SMALL_ICON_BMP,	
	MAX_QUICK_ACCESS_ITEM
};

static const __s32 lv_quick_access_res_bmp[] =
{
	ID_TEMP_1280_720_WALLPAPER_BMP,
	ID_TEMP_1280_720_IMAGE_ADD_FILL_BMP,
	ID_TEMP_1280_720_MIRRORAPP_LOGO_NOTI_96_BMP,
};

#endif

