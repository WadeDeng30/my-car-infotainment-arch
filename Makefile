INFOTAINMENT_PATH=$(srctree)/livedesk/leopard/infotainment

ccflags-y   +=  -DEPDK_DEBUG_LEVEL=EPDK_DEBUG_LEVEL_LOG_ALL         \
                -DUSED_BY_INIT                      \
                $(SOLUTION_INCLUDE) 

subdir-ccflags-y +=	-I$(srctree)/include/melis/kernel/drivers  \
		-I$(srctree)/include/melis \
		-I$(srctree)/include/melis/kernel \
		-I$(srctree)/include/melis/misc   \
		-I$(srctree)/include/melis/libs/libc \
		-I$(srctree)/include/melis/module     \
		-I$(srctree)/include/melis/elibrary/libc \
		-I$(srctree)/elibrary/lib_lvgl    \
		-I$(srctree)/elibrary/freetype/include \
		-I$(srctree)/livedesk/leopard/include \
		-I$(srctree)/livedesk/leopard/include/elibs \
		-I$(srctree)/livedesk/leopard/include/elibs/lib_ex \
		-I$(srctree)/livedesk/leopard/include/elibs/lib_ex/scanfile \
		-I$(srctree)/livedesk/leopard/include/mod_desktop/msg_srv \
		-I$(srctree)/livedesk/leopard/include/mod_desktop \
		-I$(srctree)/ekernel/drivers/rtos-hal/hal/source/g2d_rcq  \
        -I$(srctree)/ekernel/drivers/include/hal \
        -I$(srctree)/ekernel/drivers/include/drv \
		-I$(srctree)/elibrary/sft/sfte \
		-I$(srctree)/elibrary/lib_lvgl/sunxi \
		-I$(INFOTAINMENT_PATH)/bus_router \
		-I$(INFOTAINMENT_PATH)/include \
		-I$(INFOTAINMENT_PATH)/drivers \
		-I$(INFOTAINMENT_PATH)/ui/ \
		-I$(INFOTAINMENT_PATH)/ui/screens \
		-I$(INFOTAINMENT_PATH)/drivers/display \
		-I$(INFOTAINMENT_PATH)/drivers/input \
		-I$(INFOTAINMENT_PATH)/common \
		-DEPDK_DEBUG_LEVEL=EPDK_DEBUG_LEVEL_LOG_ALL -fshort-enums -std=c99

usrlibs-y +=	-L$(srctree)/${elibrary-libs}/3rd \
		-L$(srctree)/livedesk/leopard/elibs/lib_ex \
		--start-group \
		--whole-archive -lsyscall --no-whole-archive -llvgl -lcheck_app \
		 -lminic -lfreetype -lmediainfo -llzma -lelibs_ex -lsft -lcharset -lcharenc -ljpegdec\
		--end-group

MOD_NAME    := infotainment
SUF_NAME    := axf
PRJ_PATH    := $(MELIS_BASE)/projects/${RTOS_TARGET_BOARD_PATH}/data/UDISK/apps/
TEMP_PATH   := $(MELIS_BASE)/emodules/bin

$(MOD_NAME)-objs += infotainment_main.o
$(MOD_NAME)-objs += mod_infotainment.o
$(MOD_NAME)-objs += magic.o
$(MOD_NAME)-objs += built-in.o
$(MOD_NAME)-objs += bus_router/bus_router.o
obj-y += services/
obj-y += ui/
obj-y += drivers/
#obj-y += example/ common/ porting/


include $(MELIS_BASE)/scripts/Makefile.mods
