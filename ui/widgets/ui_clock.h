#ifndef _UI_CLOCK_H_
#define _UI_CLOCK_H_

enum
{
	CLOCK_IMAGE_BMP,
	MAX_CLOCK_ITEM
};

static const __s32 lv_clock_res_bmp[] =
{
	ID_TEMP_1280_720_CLOCK_BMP,
};




void ui_clock_register_init(void);

#endif
