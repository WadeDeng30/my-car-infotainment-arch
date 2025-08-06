#ifndef __UI_SOURCE_H_
#define __UI_SOURCE_H_

void ui_source_widget_init(void);

enum
{
	TUNER_SMALL_ICON_BMP,
	TUNER_FM_ICON_BG_BMP,	
	TUNER_AM_ICON_BG_BMP,	
	TUNER_ICON_BMP,
	TUNER_SET_UP_N_BMP,
	TUNER_SET_UP_P_BMP,
	TUNER_ADD_PRESET_N_BMP,
	TUNER_ADD_PRESET_P_BMP,
	TUNER_EQ_N_BMP,
	TUNER_EQ_P_BMP,
	TUNER_AUTO_STORE_N_BMP,
	TUNER_AUTO_STORE_P_BMP,
	TUNER_FREQ_SCALE_BMP,
	TUNER_PRESET_LIST_N_BMP,
	TUNER_PRESET_LIST_P_BMP,
	TUNER_PREV_N_BMP,
	TUNER_PREV_P_BMP,
	TUNER_NEXT_N_BMP,
	TUNER_NEXT_P_BMP,
	SOURCE_TUNE_BUTTON_BMP,
	SOURCE_USB_BUTTON_BMP,
	SOURCE_BT_BUTTON_BMP,
	SOURCE_MORE_BUTTON_BMP,
	SOURCE_LIST_RETURN_N_BMP,
	MAX_SOURCE_ITEM
};

static const __s32 lv_source_res_bmp[] =
{
	ID_TEMP_1280_720_TUNER_MODE_ICON_BMP,
	ID_TEMP_1280_720_TUNER_IMAGE1_BMP,
	ID_TEMP_1280_720_TUNER_IMAGE2_BMP,
	ID_TEMP_1280_720_RADIO_2_FILL_BMP,
	ID_TEMP_1280_720_SETUP_N_BMP,
	ID_TEMP_1280_720_SETUP_S_BMP,
	ID_TEMP_1280_720_ADD_PRESET_N_BMP,
	ID_TEMP_1280_720_ADD_PRESET_S_BMP,
	ID_TEMP_1280_720_EQ_N_BMP,
	ID_TEMP_1280_720_EQ_S_BMP,
	ID_TEMP_1280_720_AUTO_STORE_N_BMP,
	ID_TEMP_1280_720_AUTO_STORE_S_BMP,
	ID_TEMP_1280_720_FM_SEEK_SCALE_BMP,
	ID_TEMP_1280_720_PRESET_LIST_N_BMP,
	ID_TEMP_1280_720_PRESET_LIST_S_BMP,
	ID_TEMP_1280_720_PREVIOUS_N_BMP,
	ID_TEMP_1280_720_PREVIOUS_S_BMP,
	ID_TEMP_1280_720_NEXT_N_BMP,
	ID_TEMP_1280_720_NEXT_S_BMP,
	ID_TEMP_1280_720_RADIO_BMP,
	ID_TEMP_1280_720_USB_BMP,
	ID_TEMP_1280_720_BT_BMP,
	ID_TEMP_1280_720_MORE_BMP,
	ID_TEMP_1280_720_RETURN_N_BMP,
};

static const char* const fm_res_rds_pty_string[32] =
{
	" ",//"No PTY",//0
	"News",
	"Current Affairs",
	"Information",
	"Sport",
	"Education",
	"Drama",
	"Cultures",
	"Science",
	"Varied Speech",
	"Pop Music",
	"Rock Music",
	"Easy Listening",
	"Light Classics M",
	"Serious Classics",
	"Other Music",
	"Weather&Metr",
	"Finance",
	"Children's Progs",
	"Social Affairs",
	"Religion",
	"Phone In",
	"Travel&Touring",
	"Leisure&Hobby",
	"Jazz Music",
	"Country Music",
	"National Music",
	"Oldies Music",
	"Folk Music",
	"Documentary",
	"Alarm Test",
	"Alarm-Alarm!"
};

#ifdef NO_FM3_AM2_BAND
#define dMAX_BAND			1
#else
#define dMAX_BAND			3
#endif


enum
{
	TUNER_PLAY_WIN,
	TUNER_LIST_WIN,
	TUNER_SETUP_WIN,
	TUNER_MAX_WIN
};


#endif
