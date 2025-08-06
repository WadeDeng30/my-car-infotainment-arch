#ifndef __INFOTAINMENT_MAIN_H__
#define __INFOTAINMENT_MAIN_H__

#define THEME_DIR			"d:\\apps\\theme.bin"

#define PAGE_REG(name)\
do{\
    extern void REGISTER_##name(void);\
    REGISTER_##name();\
}while(0)

#include "lvgl.h"
#include <mod_update.h>
#include <mod_defs.h>
#include <elibs_stdlib.h>
#include <elibs_stdio.h>
#include <elibs_string.h>
#include <log.h>
#include <kconfig.h>
#include <license_check.h>
#include <kapi.h>
#include <gui_msg.h>
#include <lib_ex.h>
#include <mod_mixture.h>
#include <dfs_posix.h>
#include <desktop_api.h>
#include "msg_emit.h"



int infotainment_main(void *p_arg);

#endif

