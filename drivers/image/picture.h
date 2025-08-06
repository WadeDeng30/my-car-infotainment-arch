#ifndef __PICTURE_H__
#define __PICTURE_H__
//#include "app_root.h"
#include "lvgl.h"
// #include "apps.h"
typedef struct img_info_s
{
	lv_img_dsc_t 	img_dsc;
	lv_img_header_t img_header;
}img_info_t;

typedef struct image_list_s
{
	lv_img_dsc_t img_dsc;
	__u32 app_id;
	struct image_list_t *next;
}image_list_t;




lv_img_dsc_t img_open2(__u32 res_id);


#endif

