#ifndef __UI_SYS_CONTROL_H_
#define __UI_SYS_CONTROL_H_


void ui_sys_control_register_init(void);

enum
{
	
	VOLUME_SMALL_BMP,
	BACKLIGHT_ICON_BMP,
	PHONE_BUTTON_N_BMP,
	AUDIO_BUTTON_N_BMP,
	AUDIO_CAN_N_BMP,
	MAX_SYS_CONTROL_ITEM
};

static const __s32 lv_sys_control_res_bmp[] =
{
	ID_TEMP_1280_720_VOLUME_UP_LINE_BMP,
	ID_TEMP_1280_720_SUN_LINE_BMP,	
	ID_TEMP_1280_720_PHONE_LINE_BMP,	
	ID_TEMP_1280_720_SOUND_MODULE_LINE_BMP,	
	ID_TEMP_1280_720_QICHESUV_BMP,	
};


#endif
