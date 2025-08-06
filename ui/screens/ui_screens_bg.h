#ifndef __UI_SCREENS_BG_H_
#define __UI_SCREENS_BG_H_

#include "ui_manger.h"
#include "ui_screens.h"
#include "lvgl.h"

enum
{
	BACKGROUND_BMP,
	MAX_BACKGROUND_BMP_ITEM
};

static const __s32 lv_background_res_bmp[MAX_BACKGROUND_BMP_ITEM] =
{
	ID_TEMP_1280_720_GX4900_BG_BMP,
};

void screen_create_background(lv_obj_t* screen, ui_screen_id_t screen_id);


#endif
