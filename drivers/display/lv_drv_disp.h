#ifndef LV_DRV_DISP_H
#define LV_DRV_DISP_H

#include <stdint.h>
#include "lvgl.h"
#include "kapi.h"

#ifdef __cplusplus
extern "C" {
#endif

void lv_port_disp_init(void);
int lv_get_screen_width(void);
int lv_get_screen_height(void);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_DRV_DISP_H */