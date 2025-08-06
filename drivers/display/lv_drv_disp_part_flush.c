/**
 * @file lv_port_disp_templ.c
 *
 */

 /*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/

/*********************
 *      INCLUDES
 *********************/
#include "lv_drv_disp.h"
#include "lvgl.h"
#include "lv_conf.h"
#include "elibs_stdio.h"
#include "mod_display.h"
#include "log.h"
#include <mod_mixture.h>
#include <mod_defs.h>
#include <dfs_posix.h>

#include <sunxig2d.h>
#include "g2d_driver.h"

#if SUNXI_DISP_LAYER_MODE == SUNXI_DISP_USE_SINGLE_LAYER && SUNXI_DISP_FLUSH_MODE == SUNXI_DISP_PART_FLUSH
/*********************
 *      DEFINES
 *********************/

static int g_screen_width = 1280;
static int g_screen_height = 720;
/**********************
 *      TYPEDEFS
 **********************/




#if LV_ROTATE_DRAW_SW_BUFF


static inline void my_set_px_argb(uint8_t * buf, lv_color_t color, lv_opa_t opa)
{
    lv_color_t bg_color;
    lv_color_t res_color;
    lv_opa_t bg_opa = buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1];
#if LV_COLOR_DEPTH == 8
    bg_color.full = buf[0];
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[1]);
    if(buf[1] <= LV_OPA_MIN) return;
    buf[0] = res_color.full;
#elif LV_COLOR_DEPTH == 16
    bg_color.full = buf[0] + (buf[1] << 8);
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[2]);
    if(buf[2] <= LV_OPA_MIN) return;
    buf[0] = res_color.full & 0xff;
    buf[1] = res_color.full >> 8;
#elif LV_COLOR_DEPTH == 32
    bg_color = *((lv_color_t *)buf);
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[3]);
    if(buf[3] <= LV_OPA_MIN) return;
    buf[0] = res_color.ch.blue;
    buf[1] = res_color.ch.green;
    buf[2] = res_color.ch.red;
#endif
}



static void my_set_px_rotate90(
    struct _lv_disp_drv_t * disp_drv,
    uint8_t * buf,            // LVGL传递的buffer首地址（framebuffer/局部buffer）
    lv_coord_t buf_w,         // LVGL逻辑宽度（buffer一行像素数）
    lv_coord_t x, lv_coord_t y,
    lv_color_t color, lv_opa_t opa)
{
    // 假设LVGL逻辑为1280*720，物理为720*1280，framebuffer一维排布[720*1280]
    //const int PHYS_WIDTH = g_screen_width;
    //const int PHYS_HEIGHT = g_screen_height;
    lv_draw_ctx_t * draw_ctx = disp_drv->draw_ctx;

	const int PHYS_WIDTH = lv_area_get_height(draw_ctx->buf_area);
    const int PHYS_HEIGHT = lv_area_get_width(draw_ctx->buf_area);;

    // 计算全局LVGL逻辑坐标
    // set_px_cb中x、y是以draw_ctx->buf_area左上角为原点的
    // 通常你需要获得buf_area来加上全局偏移
    int logic_x = x;
    int logic_y = y;
    if(draw_ctx && draw_ctx->buf_area) {
        //logic_x += draw_ctx->buf_area->x1;
        //logic_y += draw_ctx->buf_area->y1;
    }

    // 顺时针旋转90度：物理(x',y') = (PHYS_WIDTH-1-logic_y, logic_x)
    int phys_x = PHYS_WIDTH - 1 - logic_y;
    int phys_y = logic_x;
    if(phys_x < 0 || phys_x >= PHYS_WIDTH || phys_y < 0 || phys_y >= PHYS_HEIGHT)
        return; // 防溢出

    // 一维寻址
    int idx = phys_y * PHYS_WIDTH + phys_x;

    // 每像素4字节
    lv_color_t *fb = (lv_color_t *)buf;
    if(opa >= LV_OPA_MAX) {
        fb[idx] = color;
    } else {
        // 透明度混合
        my_set_px_argb((uint8_t *)&fb[idx],color,opa);
        //lv_color_t bg = fb[idx];
        //fb[idx] = lv_color_mix(color, bg, opa);
    }
}
#endif








/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);


/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
static ES_FILE      * p_disp;
static __hdle lvgl_layer = NULL;

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    static lv_color_t *buf;
    buf = (lv_color_t*) esMEMS_Palloc((g_screen_width*g_screen_height*sizeof(lv_color_t)+1023)>>10 , 0);

    if (buf == NULL) {
        __log("malloc draw buffer fail\n");
        return 0;
    }

    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, g_screen_width * g_screen_height);

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
	#if LV_ROTATE_G2D
	disp_drv.hor_res = g_screen_height;
    disp_drv.ver_res = g_screen_width;
	#else
	#if LV_ROTATE_DRAW_SW_BUFF
	disp_drv.hor_res = g_screen_height;
    disp_drv.ver_res = g_screen_width;
	#else
    disp_drv.hor_res = g_screen_width;
    disp_drv.ver_res = g_screen_height;
	#endif
    #endif


    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &disp_buf;

    /*Required for Example 3)*/
    disp_drv.screen_transp = 1;
	#if (LV_ROTATE_G2D == 0)
	#if (LV_ROTATE_DRAW_SW_BUFF == 0)
    disp_drv.full_refresh = 0;
    disp_drv.sw_rotate = 1;
    disp_drv.rotated = LV_DISP_ROT_270;
    //disp_drv.rotated = LV_DISP_ROT_90;
	#else
	disp_drv.set_px_cb = my_set_px_rotate90;
	#endif
    //disp_drv.rotated = LV_DISP_ROT_270;
    #endif
    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    __s32       width, height;
    __disp_layer_info_t layer_para;
    __u64 arg[3] = {0};
    __s32 ret = 0;

	p_disp = esKSRV_Get_Display_Hld();;
	eLIBs_printf("disp_init p_disp:%d.\n",p_disp);
	if(!p_disp)
	{
		__err("open display device fail!\n");
		return EPDK_FAIL;
	}

	g_screen_width  = esMODS_MIoctrl(p_disp, MOD_DISP_GET_SCN_WIDTH, 0, 0);
    g_screen_height = esMODS_MIoctrl(p_disp, MOD_DISP_GET_SCN_HEIGHT, 0, 0);
	eLIBs_printf("disp_init g_screen_width:%d.\n",g_screen_width);
	eLIBs_printf("disp_init g_screen_height:%d.\n",g_screen_height);

	width  = g_screen_width;//esMODS_MIoctrl(p_disp, DISP_CMD_SCN_GET_WIDTH, 0, 0);
	height = g_screen_height;//esMODS_MIoctrl(p_disp, DISP_CMD_SCN_GET_HEIGHT, 0, 0);

    arg[0] = MOD_DISP_LAYER_WORK_MODE_NORMAL;
	arg[1] = 0;
    arg[2] = 0;
    lvgl_layer = esMODS_MIoctrl(p_disp, MOD_DISP_LAYER_REQUEST, 0, (void*)arg);
	eLIBs_printf("disp_init lvgl_layer:%d.\n",lvgl_layer);
	if(lvgl_layer != NULL)
	{
		__err("layer ok! lvgl_layer = %d\n",lvgl_layer);
	}
	return ;
}


void update_fb(__u32* dest_buffer, const lv_area_t * area, __u32* buf)
{
    int w = (area->x2 - area->x1 + 1);
    int h = (area->y2 - area->y1 + 1);
    int y = 0;
    int x = 0;
    __u32* p_dst = dest_buffer;
    __u32* p_src = buf;

    if(!p_dst || !p_src)
        return; // 无效指针


    if(w <= 0 || h <= 0)
        return; // 无效区域


#if LV_ROTATE_DRAW_SW_BUFF
	lv_area_t rotated_area;

    int x1 = area->x1;
	int y1 = area->y1;

	int x2 = area->x2;
	int y2 = area->y2;
    //eLIBs_printf("x1:%d x2:%d y1:%d y2:%d\n",area->x1,area->x2,area->y1,area->y2);

	rotated_area.x1 = g_screen_width -1 - y2;
	rotated_area.y1 = x1;

	rotated_area.x2 = g_screen_width -1 - y1;
	rotated_area.y2 = x2;

	w = (rotated_area.x2 - rotated_area.x1 + 1);
	h = (rotated_area.y2 - rotated_area.y1 + 1);
	
	//eLIBs_printf("222 x1:%d x2:%d y1:%d y2:%d\n",rotated_area.x1,rotated_area.x2,rotated_area.y1,rotated_area.y2);

	if(w<=0 || h<=0)
		return; 

	 for(y = rotated_area.y1; y <= rotated_area.y2; y++) {
		memcpy(&p_dst[(g_screen_width * y) + rotated_area.x1],p_src, w * 4);
        p_src += w;
    }
	 
	return; 
	 

#endif

     //eLIBs_printf("y2 - y1 = %d , x2 - x1 = %d\n",area->y2 - area->y1 ,area->x2 - area->x1);
    //eLIBs_printf("x1:%d x2:%d y1:%d y2:%d\n",area->x1,area->x2,area->y1,area->y2);
    for(y = area->y1; y <= area->y2; y++) {
#if LV_ROTATE_G2D
	   	memcpy(&p_dst[(g_screen_height * y) + area->x1],p_src, w * 4);
#else      
		memcpy(&p_dst[(g_screen_width * y) + area->x1],p_src, w * 4);
#endif
        p_src += w;
    }




}

void close_logo(void)
{
	__s32  enabled = 0;
	__hdle de_hdle = NULL;
    if (esCFG_GetKeyValue("mixture_para", "startup_logo_en", &enabled, 1) < 0)
    {
        /*Esh_Error("of_property_read disp_init.disp_epos_logo_enable fail");*/
        enabled = EPDK_FALSE;
    }

    __log("mixture_para : startup_logo_en = %d ",enabled);

    if(enabled == EPDK_TRUE)
    {
        //The boot logo is used, and there is no need to open the drive
        de_hdle = esKSRV_Get_Display_Hld();//eLIBs_fopen("b:\\DISP\\DISPLAY", "r+");
        if (de_hdle == ((ES_FILE *)0))
        {
            __err("can not open display driver");
            return -1;
        }

        uint32_t m_mid = 0;
        unsigned long mp_mod = 0;
        uint32_t        mid_mixture = 0;
        __mp        *mod_mixture = NULL;
        static  uint32_t release_flag = 0;
        __u32 arg[3];
        __log(".......backlayer_set_fb");
        if (!release_flag)
        {
            esKSRV_Get_Mixture_Hld(&m_mid, &mp_mod);
                __inf("mid = %x  mp = %x",m_mid,mp_mod);
            if(m_mid==0 || mp_mod==0)
            {
                __err("esKSRV get logo hdle failed\n");
                release_flag = 1;
                return;
            }
            mid_mixture = m_mid;
            mod_mixture = (__mp *)mp_mod;
            // qury carback and anintion are finished or not
            while(esMODS_MIoctrl(mod_mixture, MOD_MIXTURE_QURY_FINISH, 0, 0) != 1)
            {
                esKRNL_TimeDly(3);
            }
            arg[0] = STARTUP;
            esMODS_MIoctrl(mod_mixture, MOD_MIXTURE_STOP, 0, (void *)arg);
            if(mod_mixture)
            {
                //esMODS_MClose(mod_mixture);
            }
            if (mid_mixture)
            {
                //esMODS_MUninstall(mid_mixture);
            }
            release_flag = 1;
        }
    }
    else
    {
        //The boot logo is not used. You need to load the display driver and turn on the LCD
        de_hdle = eLIBs_fopen("b:\\DISP\\DISPLAY", "r+");
        if(!de_hdle)
        {
            __log("open display device fail!\n");
            return;
        }
        //Save display drive handle as global variable
        esKSRV_Save_Display_Hld(de_hdle);
        esMODS_MIoctrl(de_hdle, MOD_DISP_CMD_LCD_ON, 0, 0);
    }
}

int lv_get_screen_width(void)
{
    return g_screen_width;
}

int lv_get_screen_height(void)
{
    return g_screen_height;
}




static __s32 lv_port_disp_part_rotate(void* src, int src_width, int src_height, void* dst, int rot_angle)
{
    g2d_blt_h   blit;
    int dst_width = 0, dst_height = 0;
    int ret;
	int  h_rotate;

	
	h_rotate = open("/dev/g2d", O_WRONLY);
	printf("mixture_rotate h_rotate:%d.\n",h_rotate);
	if(h_rotate<0)
		return -1;

    memset(&blit, 0, sizeof(g2d_blt_h));
    switch (rot_angle)
    {
    case 0:
        blit.flag_h = G2D_ROT_0;
        dst_width = src_width;
        dst_height = src_height;
        break;
    case 90:
        blit.flag_h = G2D_ROT_90;
        dst_width = src_height;
        dst_height = src_width;
        break;
    case 180:
        blit.flag_h = G2D_ROT_180;
        dst_width = src_width;
        dst_height = src_height;
        break;
    case 270:
        blit.flag_h = G2D_ROT_270;
        dst_width = src_height;
        dst_height = src_width;
        break;
    
    default:
        printf("fatal error! rot_angle[%d] is invalid!\n", rot_angle);
		return -1;
        break;
    }

    blit.src_image_h.bbuff = 1;
    blit.src_image_h.use_phy_addr = 1;
    blit.src_image_h.color = 0xff;
    blit.src_image_h.format = G2D_FORMAT_ARGB8888;//g2d_fmt_enh//yuv420sp uv combine
    blit.src_image_h.laddr[0] = (unsigned int)esMEMS_VA2PA((unsigned long)src);
    blit.src_image_h.laddr[1] = 0;
    blit.src_image_h.laddr[2] = 0;//esMEMS_VA2PA((uint32_t)src+src_width * src_height * 5 / 4);
    blit.src_image_h.width = src_width;
    blit.src_image_h.height = src_height;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = 0;
    blit.src_image_h.clip_rect.y = 0;
    blit.src_image_h.clip_rect.w = src_width;
    blit.src_image_h.clip_rect.h = src_height;
    blit.src_image_h.gamut = G2D_BT709;
    blit.src_image_h.bpremul = 0;
    blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_GLOBAL_ALPHA;

    blit.dst_image_h.bbuff = 1;
    blit.dst_image_h.use_phy_addr = 1;
    blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = G2D_FORMAT_ARGB8888;
    blit.dst_image_h.laddr[0] = (unsigned int)esMEMS_VA2PA((unsigned long)dst);
    blit.dst_image_h.laddr[1] = 0;
    blit.dst_image_h.laddr[2] = 0;//esMEMS_VA2PA((uint32_t)dst+src_width * dst_height * 5 / 4);
    blit.dst_image_h.width = dst_width;
    blit.dst_image_h.height = dst_height;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = 0;
    blit.dst_image_h.clip_rect.y = 0;
    blit.dst_image_h.clip_rect.w = dst_width;
    blit.dst_image_h.clip_rect.h = dst_height;
    blit.dst_image_h.gamut = G2D_BT709;
    blit.dst_image_h.bpremul = 0;
    blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_GLOBAL_ALPHA;

    ret = ioctl(h_rotate, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret != 0)
    {
		printf("mixture_rotate g2d error!.\n");
        __err("g2d error!");
    }

    eLIBs_CleanFlushDCacheRegion(dst, dst_width * dst_height * 4);


	close(h_rotate);
	h_rotate = 0;
	
	printf("mixture_rotate g2d end!.\n");
    return ret;
}


/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
#if SUNXI_DISP_FLUSH_USE_DOUBLE_BUFFER == 1
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
    __s32		width, height;
	static __disp_layer_info_t layer_para;
    __u64 arg[3];
    __s32 ret;
	static int cnt = 0;
	// static __u32 time11,time22;
	static __disp_fb_t fb_para;
	static lv_color_t*pic = NULL;

	// time22 = esKRNL_TimeGet();

    static __u32* disp_buf[2];
    static __u32* draw_buf;
    static char draw_index = 0;
    if(cnt == 0)
    {
        cnt++;
        eLIBs_printf("palloc g_screen_width*g_screen_height*4+1023)>>10 = %d \n",(g_screen_width*g_screen_height*4+1023)>>10);
        disp_buf[0] = esMEMS_Palloc((g_screen_width*g_screen_height*4+1023)>>10, 0);
        disp_buf[1] = esMEMS_Palloc((g_screen_width*g_screen_height*4+1023)>>10, 0);
#if LV_ROTATE_G2D
        draw_buf = esMEMS_Palloc((g_screen_width*g_screen_height*4+1023)>>10, 0);
#endif
        if(disp_buf[0] == NULL || disp_buf[1] == NULL)
        {
            __err("alloc disp_bufer fail");
            return;
        }

        memset(disp_buf[0],0,(g_screen_width*g_screen_height*4+1023)>>10);
        memset(disp_buf[1],0,(g_screen_width*g_screen_height*4+1023)>>10);
#if LV_ROTATE_G2D
        memset(draw_buf,0,(g_screen_width*g_screen_height*4+1023)>>10);
#endif
        // memset(draw_buf,0,(g_screen_width*g_screen_height*4+1023)>>10);
		//lv_disp_set_rotation(lv_disp_get_default(),LV_DISP_ROT_90);

        close_logo();
    }

    if(disp_buf[0] == NULL || disp_buf[1] == NULL)
    {
        return;
    }



    if(draw_index == 0)
    {
        update_fb(disp_buf[0],area,(__u32*)color_p);
    }else
    {
        update_fb(disp_buf[1],area,(__u32*)color_p);
    }

#if LV_ROTATE_G2D
	lv_port_disp_part_rotate(disp_buf[draw_index],g_screen_height,g_screen_width,draw_buf,90);
#endif

    layer_para.mode = MOD_DISP_LAYER_WORK_MODE_NORMAL;
#if LV_ROTATE_G2D
    layer_para.fb.addr[0]       = (__u32)draw_buf;
#else
	// esMEMS_CleanFlushDCacheRegion((__u32)disp_buf[draw_index], g_screen_width*g_screen_height*4);
	 layer_para.fb.addr[0]	   = (__u32)disp_buf[draw_index];
#endif
    layer_para.fb.addr[1]       = NULL;
    layer_para.fb.addr[2]       = NULL;
    layer_para.fb.format        = DISP_FORMAT_ARGB_8888;//DISP_FORMAT_YUV420_SP_UVUV;
    layer_para.fb.seq           = DISP_SEQ_ARGB;
    layer_para.fb.mode          = DISP_MOD_INTERLEAVED;//
    layer_para.fb.br_swap       = 0;//
    layer_para.fb.cs_mode       = DISP_BT601;
    layer_para.mode             = MOD_DISP_LAYER_WORK_MODE_NORMAL;//
    layer_para.pipe             = 0;
    layer_para.prio             = 4;
    layer_para.alpha_en         = 0;
    layer_para.alpha_val        = 0xff;
    layer_para.ck_enable        = 0;
    layer_para.src_win.x        = 0;
    layer_para.src_win.y        = 0;
    layer_para.src_win.width    = g_screen_width;
    layer_para.src_win.height   = g_screen_height;
    layer_para.scn_win.x        = 0;
    layer_para.scn_win.y        = 0;
    layer_para.scn_win.width    = g_screen_width;
    layer_para.scn_win.height   = g_screen_height;
    layer_para.fb.size.width = g_screen_width;
    layer_para.fb.size.height = g_screen_height;

	//eLIBs_printf("disp_flush lvgl_layer:%d.\n",lvgl_layer);
    arg[0] = lvgl_layer;
    arg[1] = (__u32)&layer_para;
    arg[2] = 0;
    ret = esMODS_MIoctrl(p_disp,MOD_DISP_CMD_LAYER_SET_PARA,0,(void*)arg);

	//eLIBs_printf("disp_flush MOD_DISP_CMD_LAYER_SET_PARA:%d.\n",ret);


    if(draw_index == 0)
    {
        memcpy(disp_buf[1],disp_buf[0],g_screen_width*g_screen_height*4);
    }else
    {
        memcpy(disp_buf[0],disp_buf[1],g_screen_width*g_screen_height*4);
    }
    draw_index++;
    if(draw_index > 1)
        draw_index = 0;




#if 1
	{
	
		static __u32 frame_cnt = 0;
		static __u32 perf_last_time = 0;
		
		frame_cnt ++;
		if(lv_tick_elaps(perf_last_time) > 1000) {
			eLIBs_printf("disp FPS:%d..\n",frame_cnt);
			frame_cnt = 0;
			perf_last_time = lv_tick_get();
		}
	}
#endif
    lv_disp_flush_ready(disp_drv);
}
#else

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
    __s32		width, height;
	static __disp_layer_info_t layer_para;
    __u64 arg[3];
    __s32 ret;
	static int cnt = 0;
	// static __u32 time11,time22;
	static __disp_fb_t fb_para;
	static lv_color_t*pic = NULL;

	// time22 = esKRNL_TimeGet();

    static __u32* disp_buffer;
    if(cnt == 0)
    {
        cnt++;
        //eLIBs_printf("palloc g_screen_width*g_screen_height*4+1023)>>10 = %d \n",(g_screen_width*g_screen_height*4+1023)>>10);
        disp_buffer = esMEMS_Palloc((g_screen_width*g_screen_height*4+1023)>>10, 0);
        memset(disp_buffer,0,(g_screen_width*g_screen_height*4+1023)>>10);


        update_fb(disp_buffer,area,(__u32*)color_p);
        layer_para.mode = MOD_DISP_LAYER_WORK_MODE_NORMAL;
        esMEMS_CleanFlushDCacheRegion((__u64)disp_buffer, g_screen_width*g_screen_height*4);
        layer_para.fb.addr[0]       = (__u64)disp_buffer;
        layer_para.fb.addr[1]       = NULL;
        layer_para.fb.addr[2]       = NULL;
        layer_para.fb.format        = DISP_FORMAT_ARGB_8888;//DISP_FORMAT_YUV420_SP_UVUV;
        layer_para.fb.seq           = DISP_SEQ_ARGB;
        layer_para.fb.mode          = DISP_MOD_INTERLEAVED;//
        layer_para.fb.br_swap       = 0;//
        layer_para.fb.cs_mode       = DISP_BT601;
        layer_para.mode             = MOD_DISP_LAYER_WORK_MODE_NORMAL;//
        layer_para.pipe             = 0;
        layer_para.prio             = 4;
        layer_para.alpha_en         = 0;
        layer_para.alpha_val        = 0xff;
        layer_para.ck_enable        = 0;
        layer_para.src_win.x        = 0;
        layer_para.src_win.y        = 0;
        layer_para.src_win.width    = g_screen_width;
        layer_para.src_win.height   = g_screen_height;
        layer_para.scn_win.x        = 0;
        layer_para.scn_win.y        = 0;
        layer_para.scn_win.width    = g_screen_width;
        layer_para.scn_win.height   = g_screen_height;
        layer_para.fb.size.width = g_screen_width;
        layer_para.fb.size.height = g_screen_height;

        close_logo();
        arg[0] = lvgl_layer;
        arg[1] = (__u32)&layer_para;
        arg[2] = 0;
        esMODS_MIoctrl(p_disp,MOD_DISP_CMD_LAYER_SET_PARA,0,(void*)arg);
    }
    else
    {
        update_fb(disp_buffer,area,(__u32*)color_p);
    }
    lv_disp_flush_ready(disp_drv);
}

#endif

#endif