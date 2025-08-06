#include "ui_manger.h"
#include "ui_source.h"
#include "bus_router.h"
#include "lvgl.h"
#include <stdio.h>

#if 1//USE_LOG_PRINT
	#define __msg(...)			(eLIBs_printf("clock MSG:L%d:", __LINE__),				 \
													  eLIBs_printf(__VA_ARGS__)) 
#else
	#define __msg(...)
#endif


#define A_MASK 0xff000000
#define R_MASK 0x00ff0000
#define G_MASK 0x0000ff00
#define B_MASK 0x000000ff

typedef struct _ARGB8888 {
	unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
}  ARGB8888;




/**
 * @brief 创建 Source 控件时传入的配置结构体
 *
 * @details
 * 这个结构体非常简单，只告诉控件应该创建哪个音源的界面。
 * 具体的显示数据（如频率、歌曲名等）将由后端服务通过消息推送。
 */
typedef struct {
    // 指定创建时应该显示哪个音源的界面
    ui_source_type_t initial_source;

    // 此处可以预留一个 void* arg 指针，用于传递更复杂的、
    // 特定于音源的初始化参数（如果需要的话），但目前可以不用。
    // void* user_data;

} ui_source_create_config_t;

// --- 内部函数的前向声明 ---

/**
 * @brief 控件内部状态结构体
 */
typedef struct {
    lv_obj_t* container;            // 控件的主容器
	
    // FM/AM 播放器界面的 LVGL 对象指针
    
	lv_obj_t * ui_source_bg_panel;
	lv_obj_t * ui_source_bg_img;
	lv_img_dsc_t lv_bg_img;
    lv_obj_t* radio_panel;
	 lv_obj_t* radio_play_panel;
	lv_obj_t* radio_list_panel;
	lv_obj_t* radio_setup_panel;
	
	lv_obj_t * ui_tuner_small_icon;
	lv_obj_t * ui_tuner_text;
	lv_obj_t * ui_tuner_big_icon_bg;
	lv_obj_t * ui_tuner_icon;
	lv_obj_t * radio_freq_label;
	lv_obj_t * radio_band_label;
	lv_obj_t * radio_ps_name_label;
	lv_obj_t * radio_pty_name_label;
	lv_obj_t * ui_source_setup;
	lv_obj_t * ui_tuner_set_pancel;
	lv_obj_t * ui_add_preset_button;
	lv_obj_t * ui_add_preset_num;
	lv_obj_t * ui_tuner_eq_button;
	lv_obj_t * ui_auto_store_button;
	lv_obj_t * ui_tuner_list_button;
	lv_obj_t * ui_freq_panel;
	lv_obj_t * ui_freq_scale;
	lv_obj_t * ui_freq_red_scale_line;
	lv_obj_t * ui_freq_text_panel;
	lv_obj_t * ui_freq_text[512];
	lv_obj_t * ui_tuner_func_pancel;
	lv_obj_t * ui_prev_button;
	lv_obj_t * ui_band_button;
	lv_obj_t * ui_fm_text;
	lv_obj_t * ui_am_text;
	lv_obj_t * ui_next_button;
	lv_obj_t * ui_source_sel_pancel;
	lv_obj_t * ui_source_tuner_button;
	lv_obj_t * ui_source_usb_button;
	lv_obj_t * ui_source_bt_button;
	lv_obj_t * ui_source_more_button;
    // (未来可添加 ST, LOC 等指示灯对象)

	
	lv_obj_t* radio_list_band_button;
	lv_obj_t* radio_list_back_button;
	lv_obj_t * ui_list_fm_text;
	lv_obj_t * ui_list_am_text;
	lv_obj_t* radio_list;

	
	HTHEME lv_source_bmp[MAX_SOURCE_ITEM];
	lv_img_dsc_t lv_source_icon[MAX_SOURCE_ITEM];
	__u32  freq_scale_total_step;
	__s32 freq_scale_start_x;
	__u32 freq_pancel_width;
	__u32 freq_scale_1step_width;
	    lv_coord_t freq_scale_img_w;
	    lv_coord_t freq_scale_setp_w;
	    lv_coord_t freq_scale_w;
	    lv_coord_t freq_panel_w;
	__u32  frq_cnt;
	__u32 cur_band;
	__u8 cur_tuner_win;
	
	lv_obj_t * fm_menu_obj;
	explr_list_para_t ui_list_para;

    // 其他音源的面板指针 (占位)
    lv_obj_t* dab_panel;
    lv_obj_t* bt_panel;
    lv_obj_t* usb_panel;
	__u8 *pic_out_bg_buf;
	__u8 * lv_bg_buffer;
	

} ui_source_widget_state_t;


// --- 静态全局变量 ---

// 控件状态的静态实例
static ui_source_widget_state_t g_source_state;

static ui_source_create_config_t create_cfg = {.initial_source  = UI_SOURCE_TYPE_FM}; // 默认创建 FM 音源界面




static lv_obj_t* source_widget_create(lv_obj_t* parent, void* arg);
static void source_widget_destroy(ui_manger_t* manger);
static int source_widget_handle_event(ui_manger_t* manger, const app_message_t* msg);
static void create_fm_am_panel(lv_obj_t* parent);
static void create_dab_panel(lv_obj_t* parent); // 占位
static void create_bt_panel(lv_obj_t* parent);  // 占位
static void create_usb_panel(lv_obj_t* parent); // 占位
static void switch_source_panel(ui_source_type_t new_source);
static void refresh_radio_ui_from_cache(void);
static void ui_tuner_update_band(void);
static void ui_tuner_clear_freq_scale(void);
static void ui_tuner_show_open_win_by_type(ui_source_widget_state_t *source_para, __u8 bwin_type);
static void ui_tuner_funtion_listbar_uninit(ui_source_widget_state_t *source_para);
static void ui_tuner_funtion_listbar_init(ui_source_widget_state_t *source_para);





/**
 * @brief 1. 【核心修改】直接在文件作用域定义并初始化各种音源的默认数据
 *
 * 这些变量作为编译时确定的默认值。
 */
static radio_state_payload_t g_radio_data_cache = {
    .preset_index = 0,
    .frequency = 87500, // 默认 87.5 MHz
    .ps_name = " ",
    .band = 0,
    .pty = 0,
    .max_freq = 108000,
    .min_freq = 87500,
    .tu_step = 10,
};



lv_img_dsc_t * ui_source_get_res(ui_source_widget_state_t * source_para, __u32 icon)
{
	__u32 bmp_num = sizeof(lv_source_res_bmp)/sizeof(__s32);
	
	if(bmp_num != MAX_SOURCE_ITEM)
	{
		__err("error MAX_source_ITEM=%d, bmp_num=%d, icon=%d !!!\n", MAX_SOURCE_ITEM, bmp_num, icon);
		if(icon >= bmp_num)
		{
			return NULL;
		}
	}

	if((lv_source_res_bmp[icon] == NULL) || icon > (MAX_SOURCE_ITEM - 1))
		return NULL;

	if(source_para->lv_source_icon[icon].data == NULL)
	{
		Lzma_get_theme(lv_source_res_bmp[icon], &source_para->lv_source_bmp[icon]);
		//eLIBs_printf("1:lv_bt_bmp[%d]=0x%x, lv_bt_res_bmp=0x%x\n", icon, bt_para->ui_bt_res_para.lv_bt_bmp[icon], lv_bt_res_bmp[icon]);
		Lzma_get_theme_buf(lv_source_res_bmp[icon], &source_para->lv_source_bmp[icon], &source_para->lv_source_icon[icon]);
	}

	return &source_para->lv_source_icon[icon];
}

void ui_source_uninit_res(ui_source_widget_state_t * source_para)
{
	__u32 	i;
	
	for( i = 0; i < MAX_SOURCE_ITEM; i++ )
	{
		if( source_para->lv_source_bmp[i] )
		{
			dsk_theme_close( source_para->lv_source_bmp[i] );
			source_para->lv_source_bmp[i] = NULL;
		}
	}
}

static int bound_pixel(int val)
{
	if (val >= 0 && val <= 255)
		return val;
	else
		return (val < 0) ? 0 : 255;
}


static int YV12_Scalar_To_RGB32_aligned(unsigned char* srcChartYAddr, unsigned char* srcChartUAddr,
                                        unsigned char* srcChartVAddr, __u32* dstArgbAddr,
                                    	const int srcChartWidth,const  int srcChartHeight,
                                    	const int dstChartWidth,const  int dstChartHeight)
{
	const int aligned_height = (srcChartHeight % 16) == 0 ? srcChartHeight : ((srcChartHeight >> 4) + 1) << 4;
	const int aligned_width = (srcChartWidth % 16) == 0 ? srcChartWidth : ((srcChartWidth >> 4) + 1) << 4;
	const long nYLen = (long)(aligned_height * aligned_width);
	const int nHalfWidth = aligned_width >> 1;
	unsigned char* yData = srcChartYAddr;//srcChartAddr;
	unsigned char* vData = srcChartUAddr;//&yData[nYLen];
	unsigned char* uData = srcChartVAddr;//&vData[nYLen >> 2];
	unsigned short src_h = 0, src_w = 0;
	unsigned int src_row = 0, dst_row = 0;
	unsigned int h_weight = 0, w_weight = 0;
	unsigned int h_step = 0, w_step = 0;
	//unsigned int diff = 0, div = 0, mod = 0;
	const  int Alpha1 = 1.370705 * 1024 * 1024;
	const  int Alpha2_1 = 0.698001 * 1024 * 1024;
	const  int Alpha2_2 = 0.703125 * 1024 * 1024;
	const  int Alhpa3 = 1.732446 * 1024 * 1024;
	int u_diff = 0;
	int v_diff = 0;
	int i, j;
	int src_row_u = 0;
	int src_w_u = 0;
	int src_h_u = 0;
	int w_weight_u = 0;
	int h_weight_u = 0;
	__u32 A,R,G,B;
	

    //eLIBs_printf("srcChartYAddr:%p, srcChartUAddr:%p, srcChartVAddr:%p\n", srcChartYAddr, srcChartUAddr, srcChartVAddr);
    //eLIBs_printf("srcChartHeight:%d, srcChartWidth:%d\n", srcChartHeight, srcChartWidth);
    //eLIBs_printf("dstChartHeight:%d, dstChartWidth:%d\n", dstChartHeight, dstChartWidth);

    if (!uData || !vData)  
        return -1;

    h_step = ((srcChartHeight) << 10) / (dstChartHeight);
	w_step = ((srcChartWidth) << 10) / (dstChartWidth);

    __msg("h_step:%u, w_step:%u\n", h_step, w_step);
	//double t1 = cv::getTickCount();
	for (i = 0; i < dstChartHeight; i++) {
		dst_row = i * dstChartWidth;
		src_row = src_h * aligned_width;
		w_weight = 0;
		src_w = 0;


		src_row_u = src_h_u *nHalfWidth;
		w_weight_u = 0;
		src_w_u = 0;

		for (j = 0; j < dstChartWidth; j++) {
			int src_Y_idx = src_row + src_w;
			int src_U_idx = src_row_u + src_w_u;
			v_diff = vData[src_U_idx] - 128;
			u_diff = uData[src_U_idx] - 128;
		
			//B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b??????  
			//G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g??????  
			//R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r??????  

			R = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b??????  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g??????  
			B = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r??????  
			A =255;

			*dstArgbAddr = (__u32)((0xff000000 & A_MASK) | ((R << 16) & R_MASK) | ((G << 8) & G_MASK) | ((B << 0) & B_MASK)); 
			w_weight += w_step;
			src_w = w_weight >> 10;

			w_weight_u += (w_step >> 1);
			src_w_u = w_weight_u >> 10;
			dstArgbAddr++;
		}
		h_weight += h_step;
		src_h = h_weight >> 10;

		h_weight_u += (h_step >> 1);
		src_h_u = h_weight_u >> 10;
	}

	return 0;
}

static int YV444_Scalar_To_RGB32_aligned(unsigned char* srcChartYAddr, unsigned char* srcChartUAddr,
                                        unsigned char* srcChartVAddr, __u32* dstChartAddr,
                                    	const int srcChartWidth,const  int srcChartHeight,
                                    	const int dstChartWidth,const  int dstChartHeight)
{
	/*����ģʽ�������YUV444û��16���룬�������Ҳ��Ҫ���룬����ͼƬ��ʾ������*/
	//const int aligned_height = (srcChartHeight % 16) == 0 ? srcChartHeight : ((srcChartHeight >> 4) + 1) << 4;
	//const int aligned_width = (srcChartWidth % 16) == 0 ? srcChartWidth : ((srcChartWidth >> 4) + 1) << 4;
	const int aligned_height =  srcChartHeight;
	const int aligned_width = srcChartWidth;
	const long nYLen = (long)(aligned_height * aligned_width);

	unsigned char* yData = srcChartYAddr;//srcChartAddr;
	unsigned char* vData = srcChartUAddr;//&yData[nYLen];
	unsigned char* uData = srcChartVAddr;//&vData[nYLen];
    
	unsigned short src_h = 0, src_w = 0;
	unsigned int src_row = 0, dst_row = 0;
	unsigned int h_weight = 0, w_weight = 0;
	unsigned int h_step = 0, w_step = 0;

	const  int Alpha1 = 1.370705 * 1024 * 1024;
	const  int Alpha2_1 = 0.698001 * 1024 * 1024;
	const  int Alpha2_2 = 0.703125 * 1024 * 1024;
	const  int Alhpa3 = 1.732446 * 1024 * 1024;
	int u_diff = 0;
	int v_diff = 0;
	int i, j;
	__u32 A=0,R=0,G=0,B=0;
    
    if (!uData || !vData)  
        return -1;

	h_step = ((srcChartHeight) << 10) / (dstChartHeight);
	w_step = ((srcChartWidth) << 10) / (dstChartWidth);

    __wrn("h_step:%u, w_step:%u\n", h_step, w_step);

	for (i = 0; i < dstChartHeight; i++) {
		dst_row = i * dstChartWidth;
		src_row = src_h * aligned_width;
		w_weight = 0;
		src_w = 0;

		for (j = 0; j < dstChartWidth; j++) {
			int src_Y_idx = src_row + src_w;
			int src_U_idx = src_Y_idx;
			v_diff = vData[src_U_idx] - 128;
			u_diff = uData[src_U_idx] - 128;

			//dstChartAddr[dst_row + j].b = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b����ֵ  
			//dstChartAddr[dst_row + j].g = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g����ֵ  
			//dstChartAddr[dst_row + j].r = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r����ֵ  
			//dstChartAddr[dst_row + j].a = 255;

			B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b����ֵ  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g����ֵ  
			R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r����ֵ  
			A = 255;
			
			*dstChartAddr = (__u32)((0xff000000 & A_MASK) | ((R << 16) & R_MASK) | ((G << 8) & G_MASK) | ((B << 0) & B_MASK)); 
			
			w_weight += w_step;
			src_w = w_weight >> 10;
			dstChartAddr++;

		}
		h_weight += h_step;
		src_h = h_weight >> 10;
	}
    
	return 0;
}






static int ARGB8888_Scaler_chart(ARGB8888 *srcChartAddr, unsigned short srcChartWidth, unsigned short srcChartHeight,
                          ARGB8888 *dstChartAddr, unsigned short dstChartWidth, unsigned short dstChartHeight)
{
    unsigned short i, j;
    unsigned short src_h = 0, src_w = 0;
    unsigned int src_row = 0, dst_row = 0;
    unsigned int h_weight = 0, w_weight = 0;
    unsigned int h_step = 0, w_step = 0;
    unsigned int diff = 0, div = 0, mod = 0;

    if ((srcChartAddr == NULL) ||
        (dstChartAddr == NULL) ||
        (srcChartWidth == 0) ||
        (srcChartHeight == 0) ||
        (dstChartWidth == 0) ||
        (dstChartHeight == 0))
    {
        return -1;
    }

    if ((srcChartWidth == dstChartWidth) && (srcChartHeight == dstChartHeight))
    {
        memcpy((void *)dstChartAddr, (const void *)srcChartAddr, srcChartWidth * srcChartHeight * 4);
        return 0;
    }

    h_step = (srcChartHeight << 10) / dstChartHeight;
    w_step = (srcChartWidth << 10) / dstChartWidth;

    for (i = 0; i < dstChartHeight; i++)
    {
        dst_row = i * dstChartWidth;
        src_row = src_h * srcChartWidth;
        w_weight = 0;
        src_w = 0;

        for (j = 0; j < dstChartWidth; j++)
        {
            /*  dstChartAddr[dst_row + j].r = srcChartAddr[src_row + src_w].r;
                dstChartAddr[dst_row + j].g = srcChartAddr[src_row + src_w].g;
                dstChartAddr[dst_row + j].b = srcChartAddr[src_row + src_w].b;
                dstChartAddr[dst_row + j].a = srcChartAddr[src_row + src_w].a;*/
            *((int *)&dstChartAddr[dst_row + j].b) = *((int *)&srcChartAddr[src_row + src_w].b);
            dstChartAddr[dst_row + j].a = 0xff;
            w_weight += w_step;
            src_w = w_weight >> 10;
        }

        h_weight += h_step;
        src_h = h_weight >> 10;
    }

    return 0;
}



#if 0

int ui_source_Tinyjpeg_dec(__u8 *pic_out_buf,__u8 *buff,__u32 buff_Size,__u32 PIC_WIDTH,__u32 PIC_HEIGHT)
{
	unsigned int length_of_file = 0;
	unsigned int width= 0, height= 0;
	unsigned char *buf = NULL;
	struct jdec_private *jdec = NULL;
	unsigned char *components[3];
	char *argb_buf = NULL;

	__msg("11111..\n");

	if(pic_out_buf == NULL || buff == NULL)
		return EPDK_FAIL;
	__msg("22222..\n");

	if(buff_Size == 0)
		return EPDK_FAIL;
	__msg("33333..\n");

	if(PIC_WIDTH == 0 || PIC_HEIGHT == 0)
		return EPDK_FAIL;
	
	__msg("44444..\n");

	length_of_file = buff_Size;

	buf = (unsigned char *)buff;
	if(buf == NULL)
		return EPDK_FAIL;

	
	__msg("55555..\n");
	/* Decompress it */
	jdec = tinyjpeg_init();
	if (jdec == NULL)
	{
		printf("Not enough memory to alloc the structure need for decompressing\n");
		goto dec_err;
	}
	__msg("666666..\n");

	if (tinyjpeg_parse_header(jdec, buf, length_of_file) < 0)
	{
		printf(tinyjpeg_get_errorstring(jdec));
		__msg("6.6.6.6.6.6..\n");
		goto dec_err;
	}
	__msg("77777..\n");

	/* Get the size of the image */
	tinyjpeg_get_size(jdec, &width, &height);

	if(width == 0 || height == 0)
		goto dec_err;
		
	__msg("88888..\n");

	printf("Decoding JPEG image w:%d,h:%d...\n",width,height);
	if (tinyjpeg_decode(jdec, TINYJPEG_FMT_RGB24) < 0)
	{
		printf(tinyjpeg_get_errorstring(jdec));
		goto dec_err;
	}

	/*
	 * Get address for each plane (not only max 3 planes is supported), and
	 * depending of the output mode, only some components will be filled
	 * RGB: 1 plane, YUV420P: 3 planes, GREY: 1 plane
	 */
	tinyjpeg_get_components(jdec, components);

	#if 0
	//write_yuv("f:\\wallpaper",width,height,components);
	write_tga("f:\\wallpaper.RGB",TINYJPEG_FMT_RGB24,width,height,components);
	#endif

	argb_buf = (char *)malloc(width*height*4);

	if(argb_buf == NULL)
	{
		goto dec_err;
	}
		
	RGB888_to_ARGB8888((char *)components[0],(char *)argb_buf,width,height);
	ARGB8888_Scaler_chart(argb_buf,width,height,pic_out_buf,PIC_WIDTH,PIC_HEIGHT);

	if(argb_buf)
		free(argb_buf);

	/* Only called this if the buffers were allocated by tinyjpeg_decode() */
	tinyjpeg_free(jdec);
	/* else called just free(jdec); */
	printf("Decoding JPEG image succeed...\n");

	return 0;

dec_err:

	if(argb_buf)
		free(argb_buf);


	if(jdec)
		tinyjpeg_free(jdec);
		


	return EPDK_FAIL;


	
}
#endif



static  __s32 source_big_icon_tag_data_converter(__u8 *pic_out_buf,__u8 *buff,__u32 buff_Size,__u32 PIC_WIDTH,__u32 PIC_HEIGHT)
{
	int ret = EPDK_FAIL;
	__mp *de_hdle = NULL;
	__u8 *pbuf = NULL;//bt_para->pIosAccess->ucArtworkData;
	__u32 wArtworkSize;
	
	if(pic_out_buf == NULL)
	{
		__msg("palloc fail!!\n");
		goto err_out;
	}

	if(buff == NULL)
	{
		__msg("palloc fail!!\n");
		goto err_out;
	}

	de_hdle = esKSRV_Get_Display_Hld();
	if(!de_hdle)
	{
		__msg("open display device fail!\n");
		goto err_out;
	}	

	{
		__u8 *picture_buf = NULL;
		__u32 n=0;
    		__u32 picture_size = 0;
		ES_FILE    *fp = NULL;
		__u32 show_buf_len;
		__u32 arg[3];
		__s32 scale_handle;
		__disp_scaler_para_t param;
		__willow_buf_info_t in_para;
		__willow_output_t out_para;
		__s32 pic_size = 0;
		__u32 start_data;
		__u32 i =0;
		
		pic_size = buff_Size;
		
		eLIBs_memset(&in_para, 0, sizeof(__willow_buf_info_t));
    		eLIBs_memset(&out_para, 0, sizeof(__willow_output_t));
	
		__msg("pic_size=%d...\n",pic_size);

		in_para.buffer = buff;
		in_para.buffersize = pic_size;

		__msg("rat_start_miniature_decode\n");
		ret = rat_start_miniature_decode();
		__msg("ret=%d...\n",ret);

		__msg("in_para.buffer=%x..\n",in_para.buffer);
		__msg("in_para.buffersize=%d..\n",in_para.buffersize);
		
		ret = rat_get_pic_info_from_buf(&in_para, &out_para);	
		
		__msg("ret=%d...\n",ret);
		__msg("%x, %d, %d, %d, %d\n",out_para.is_supported,out_para.img_fb.size.width,out_para.img_fb.size.height,out_para.img_fb.fmt.type,out_para.img_fb.fmt.fmt.rgb.pixelfmt); 
		//conver to disp fb
		if(ret == EPDK_OK)
		{

			//memcpy((__u32 *)pic_out_buf,(__u32 *)out_para.img_fb.addr[0],out_para.img_fb.size.width*out_para.img_fb.size.height*4);

			if(out_para.img_fb.fmt.type == FB_TYPE_YUV)
			{
				if(out_para.img_fb.fmt.fmt.yuv.pixelfmt == PIXEL_YUV444)
				{
					ret = YV444_Scalar_To_RGB32_aligned((__u8 *)out_para.img_fb.addr[0],(__u8 *)out_para.img_fb.addr[1],
														(__u8 *)out_para.img_fb.addr[2],(__u32 *)pic_out_buf,
														out_para.img_fb.size.width,out_para.img_fb.size.height,
														PIC_WIDTH,PIC_HEIGHT);
				}
				else
				{
					ret = YV12_Scalar_To_RGB32_aligned((__u8 *)out_para.img_fb.addr[0],(__u8 *)out_para.img_fb.addr[1],
														(__u8 *)out_para.img_fb.addr[2],(__u32 *)pic_out_buf,
														out_para.img_fb.size.width,out_para.img_fb.size.height,
														PIC_WIDTH,PIC_HEIGHT);
				}
			}
			else
			{
				ARGB8888_Scaler_chart((__u8 *)out_para.img_fb.addr[0],out_para.img_fb.size.width,out_para.img_fb.size.height,pic_out_buf,PIC_WIDTH,PIC_HEIGHT);
			}


			
			
			

		
			/*param.input_fb.addr[0] = (__u32)out_para.img_fb.addr[0];
			param.input_fb.addr[1] = (__u32)out_para.img_fb.addr[1];
			param.input_fb.addr[2] = (__u32)out_para.img_fb.addr[2];
			param.input_fb.size.width = out_para.img_fb.size.width;
			param.input_fb.size.height = out_para.img_fb.size.height;
			if(out_para.img_fb.fmt.type == FB_TYPE_YUV)
			{
				param.input_fb.format = out_para.img_fb.fmt.fmt.yuv.pixelfmt-5;
				param.input_fb.seq = yuv_seq_converter(out_para.img_fb.fmt.fmt.yuv.yuvseq);//DISP_SEQ_UVUV;//
				param.input_fb.mode = yuv_mode_converter(out_para.img_fb.fmt.fmt.yuv.mod);//DISP_MOD_MB_UV_COMBINED;//
				param.input_fb.br_swap = 0;
				param.input_fb.cs_mode = out_para.img_fb.fmt.cs_mode;
				__bt_msg("pixelfmt:%x, seq:%x ,mode:%x, cs:%x\n",out_para.img_fb.fmt.fmt.yuv.pixelfmt,
					out_para.img_fb.fmt.fmt.yuv.yuvseq,out_para.img_fb.fmt.fmt.yuv.mod,
					out_para.img_fb.fmt.cs_mode);
			}
			else //RGB
			{
				param.input_fb.format = out_para.img_fb.fmt.fmt.rgb.pixelfmt;
				param.input_fb.seq = rgb_seq_converter(out_para.img_fb.fmt.fmt.rgb.pixseq);
				param.input_fb.mode = DISP_MOD_INTERLEAVED;//0;
				param.input_fb.br_swap = out_para.img_fb.fmt.fmt.rgb.br_swap;
				param.input_fb.cs_mode = out_para.img_fb.fmt.cs_mode;
			}

			show_buf_len = PIC_WIDTH*PIC_HEIGHT;
			show_buf_len = (show_buf_len + 1023)/1024*1024;
			param.output_fb.addr[0] = (__u32)pic_out_buf;
			param.output_fb.addr[1] = (__u32)(param.output_fb.addr[0]+show_buf_len);
			param.output_fb.addr[2] = (__u32)(param.output_fb.addr[1]+show_buf_len);
			param.output_fb.size.width = PIC_WIDTH;
			param.output_fb.size.height = PIC_HEIGHT;
			param.output_fb.format = DISP_FORMAT_ARGB8888;
			param.output_fb.seq = 0;
			param.output_fb.mode = DISP_MOD_INTERLEAVED;
			param.output_fb.br_swap = 0;
			param.output_fb.cs_mode = 0;
			
			scale_handle = eLIBs_fioctrl(de_hdle, DISP_CMD_SCALER_REQUEST, 0, NULL);
			if(scale_handle > 0)
			{
		    	 __here__;
		        param.source_regn.x      = 0;
		        param.source_regn.y      = 0;
		        param.source_regn.width  = param.input_fb.size.width;
		        param.source_regn.height = param.input_fb.size.height;
		        arg[0] = scale_handle;
		        arg[1] = (__u32)&param;
		        arg[2] = 0;
		        ret = eLIBs_fioctrl(de_hdle, DISP_CMD_SCALER_EXECUTE, 0, arg);
				__bt_msg("scaler ret= %d\n",ret);
		        if (ret == EPDK_FAIL)
		        {
		            __bt_msg("scale image err!\n");          
		        }
				arg[0] = scale_handle;
				arg[1] = 0;
				arg[2] = 0;
		        eLIBs_fioctrl(de_hdle, DISP_CMD_SCALER_RELEASE, 0, arg);
			  }
			  else
			  {
					__bt_msg("request scaler fail !!\n");

					if(de_hdle)
						eLIBs_fclose(de_hdle);

					rat_stop_miniature_decode();
					return -2; //decode err
			  }*/

		}
		__msg("ret= %d.....\n",ret);
		rat_stop_miniature_decode();	

		/*if(de_hdle)
			eLIBs_fclose(de_hdle);*/

		__msg("ret= %d.....\n",ret);
		if(ret == EPDK_OK)
			return EPDK_OK;
		else 
			return -2; //decode err
	}

	if(de_hdle)
	{
		//eLIBs_fclose(de_hdle);
		de_hdle = NULL;
	}
	return EPDK_OK;
	
err_out:

	if(de_hdle)
	{
		//eLIBs_fclose(de_hdle);
		de_hdle = NULL;
	}
	return EPDK_FAIL;
}


///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void source_bgd_set_img_src_buff(ui_source_widget_state_t * source_para,char *pic_buf,__s32 width,__s32 height)
{
	__u32 i=0;
	__s32 ret;
	__s32 w,h;
    SIZE screen_size;
	__disp_scaler_para_t param;
	int BytesPerLine;
	__u32 zoom_buf_len=0;
	__u32 *temp_bg_buf=NULL;
	char *temp_pic_buf=NULL;
	__u32 *src_buf=NULL;
	__u32 *det_buf=NULL;

	if(!pic_buf)
		return EPDK_FAIL;

	if(width==0 ||height == 0)
		return EPDK_FAIL;

	BytesPerLine = ((32 * width + 31) >> 5) << 2;
	temp_pic_buf = (char *)esMEMS_Palloc((width*height*4+1023)/1024, 0);

	if(temp_pic_buf == NULL)
	{
		return EPDK_FAIL;
	}
	else
	{
		memcpy(temp_pic_buf,pic_buf,width*height*4);
	}
	
	__msg("temp_pic_buf=%x..\n",temp_pic_buf);	
	__msg("width=%d..\n",width);
	__msg("height=%d..\n",height);
	GaussianBlur(width,height,BytesPerLine,SIGMA,(int *)temp_pic_buf,(int *)temp_pic_buf);



	__msg("screen_size.width=%d..\n",screen_size.width);
	__msg("screen_size.height=%d..\n",screen_size.height);
		
	{
		if(!source_para->lv_bg_buffer)
		{
			source_para->lv_bg_buffer = esMEMS_Palloc(4 * (width) * (height) / 1024+ 1, 0);

			if(source_para->lv_bg_buffer == NULL)
			{
				goto set_bg_error;
			}
		}
		
		/*if(ui_background_para->lv_bg_img.data)
		{
			if(ui_background_para->lv_bg_img.data != ui_background_para->lv_bg_buffer)
			{
				esMEMS_Bfree(ui_background_para->lv_bg_img.data, ui_background_para->lv_bg_img.data_size);
				data_del_tatol += ui_background_para->lv_bg_img.data_size; 
				ui_background_para->lv_bg_img.data = NULL;
			}
		}*/
		temp_bg_buf = (__u32 *)source_para->lv_bg_buffer;
		src_buf = (__u32 *)temp_pic_buf;
		det_buf = temp_bg_buf;

		memcpy(det_buf, src_buf, width*height*4);
		/*for (i=0; i<height; i++)
		{
			memcpy(det_buf, src_buf, width*4);
			det_buf =	det_buf + screen_size.width;
			src_buf  =  src_buf + width;
		}*/
		source_para->lv_bg_img.header.always_zero = 0;
		source_para->lv_bg_img.header.w = width;
		source_para->lv_bg_img.header.h = height;
		source_para->lv_bg_img.data_size = width * height * 4;
		source_para->lv_bg_img.header.cf = LV_IMG_CF_TRUE_COLOR;
		source_para->lv_bg_img.data = (void *)source_para->lv_bg_buffer;
		lv_img_set_src(g_source_state.ui_source_bg_img, &source_para->lv_bg_img);
		lv_img_set_zoom(g_source_state.ui_source_bg_img, 4*256);
	}
	

	
	

	set_bg_error:
	if(temp_pic_buf)	
	{
		esMEMS_Pfree(temp_pic_buf, (width*height*4+1023)/1024);
		temp_pic_buf = NULL;
	}


	//__bg_msg("buffer_default_tatol = %d, buffer_default_del_tatol = %d\n", buffer_default_tatol, buffer_default_del_tatol);
	return ret;
}




__s32 source_Draw_album_tags_pg(ui_source_widget_state_t * source_para,lv_obj_t * parent,__u8*pic_buf)
{
	int ret = 0;
 	void *pBuf= NULL;
	rat_miniature_para_t in_para;
	rat_audio_info_t out_para;
	RECT miniature_rect;
	lv_obj_update_layout(parent);
	lv_coord_t w  =lv_obj_get_width(parent);
	lv_coord_t h  =lv_obj_get_height(parent);
	   

	miniature_rect.width = w;
	miniature_rect.height = h;
	in_para.format = PIXEL_COLOR_ARGB8888;
	in_para.width = miniature_rect.width;
	in_para.height = miniature_rect.height;
	in_para.mode =  WILLOW_SCALE_STRETCH;//1;//0;	// 1Ϊ����ģʽ

	if(!source_para->pic_out_bg_buf)
	{
		pBuf = esMEMS_Palloc(((in_para.width*in_para.height*4)+1023)/1024, 0);
		if(pBuf == NULL) //可以放到开始申请，退出时释放
		{		
			
			return EPDK_FAIL;
		}
		
		eLIBs_memset(pBuf, 0, in_para.width*in_para.height*4);
		source_para->pic_out_bg_buf = pBuf;
	}
	#if 0//def USB_TINYJPEG_DEC
	ret = ui_source_Tinyjpeg_dec(source_para->pic_out_bg_buf,pic_buf,sizeof(pic_buf),w,h);
	#else
	ret = source_big_icon_tag_data_converter(source_para->pic_out_bg_buf,pic_buf,sizeof(pic_buf),w,h);
	#endif
	if(ret == EPDK_OK)
	{
		if(sizeof(pic_buf))
		{

			source_bgd_set_img_src_buff(source_para,source_para->pic_out_bg_buf,in_para.width,in_para.height);    

		}
		else
		{

			if(pBuf)
			{
	        		esMEMS_Pfree(pBuf, ((in_para.width*in_para.height*4)+1023)/1024);
				source_para->pic_out_bg_buf = NULL;
	        }
			return EPDK_FAIL;
		}
	}
	else
	{

		if(pBuf)
		{
			esMEMS_Pfree(pBuf, ((in_para.width*in_para.height*4)+1023)/1024);
			source_para->pic_out_bg_buf = NULL;
		}
	}
		
	return EPDK_OK;
}








#if 1



lv_color_t * ui_source_canvas_buf = NULL;
lv_img_dsc_t* ui_source_canvas_snapshot = NULL;

void ui_canvas_blur_obj(lv_obj_t* obj, lv_obj_t * parent,lv_obj_t* canvas, uint16_t blur_radius) 
{
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
	//img_dsc.opa = 255;
	img_dsc.zoom = 256*5;
	 if(ui_source_canvas_snapshot)
	    {
	        lv_snapshot_free(ui_source_canvas_snapshot);
	        ui_source_canvas_snapshot = NULL;
	    }
    if(ui_source_canvas_snapshot == NULL)
        ui_source_canvas_snapshot = lv_snapshot_take(obj, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_coord_t coord_ext_size = _lv_obj_get_ext_draw_size(parent);
    eLIBs_printf("lv_canvas_draw_img:x=%d, y=%d", parent->coords.x1 - coord_ext_size, parent->coords.y1 - coord_ext_size);
	lv_coord_t w  =lv_obj_get_width(parent);
	lv_coord_t h  =lv_obj_get_height(parent);

    //lv_canvas_draw_img(canvas, 0, 400, ui_source_canvas_snapshot, &img_dsc);
    lv_canvas_draw_img(canvas, -220, -90, ui_source_canvas_snapshot, &img_dsc);
	  //lv_canvas_transform(canvas, ui_source_canvas_snapshot, 0, 1000, 0, 0, 0, 0, true);
	lv_area_t area = { parent->coords.x1 - coord_ext_size, parent->coords.y1 - coord_ext_size, parent->coords.x2 + coord_ext_size, parent->coords.y2 + coord_ext_size };
    eLIBs_printf("area:x1=%d, y1=%d, x2=%d, y2=%d", area.x1, area.y1, area.x2, area.y2);
	 // lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);

    //lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    //lv_canvas_blur_hor(canvas, &area, blur_radius);
    //lv_canvas_blur_ver(canvas, &area, blur_radius);
    //lv_canvas_box_blur(canvas, &area, blur_radius);
    lv_canvas_stackblur_rgba(canvas, &area, blur_radius, 0);
}

lv_obj_t * ui_canvas_blur_init(lv_obj_t * parent, lv_obj_t * source, uint16_t blur_radius)
{
    //eLIBs_printf("ui_canvas_blur_init esMEMS_FreeMemSize=%d\n", esMEMS_FreeMemSize());
    lv_obj_t* ui_source_canvas = lv_canvas_create(parent);
	lv_coord_t w;
	lv_coord_t h;
	lv_obj_update_layout(parent);
	w  =lv_obj_get_width(parent);
	h  =lv_obj_get_height(parent);
	   eLIBs_printf("w=%d,h=%d\n", w,h);
    if(ui_source_canvas_buf == NULL)
    {
        //eLIBs_printf("ui_source_canvas_buf esMEMS_Palloc\n");
        ui_source_canvas_buf = (lv_color_t*)esMEMS_Palloc((LV_IMG_BUF_SIZE_TRUE_COLOR_ALPHA(w, h)+1023)>>10, 0);
    }
    lv_canvas_set_buffer(ui_source_canvas, ui_source_canvas_buf, w, h, LV_IMG_CF_TRUE_COLOR_ALPHA);

    ui_canvas_blur_obj(source,parent, ui_source_canvas, blur_radius);
    //eLIBs_printf("ui_canvas_blur_init esMEMS_FreeMemSize1=%d\n", esMEMS_FreeMemSize());
    return ui_source_canvas;
}

lv_obj_t * ui_canvas_blur_uninit(lv_obj_t * obj)
{
    //eLIBs_printf("ui_canvas_blur_uninit esMEMS_FreeMemSize=%d\n", esMEMS_FreeMemSize());
   /* if(obj)
    {
        lv_obj_refresh_ext_draw_size(obj);            // ??????
        lv_obj_del(obj);
    }*/

    if(ui_source_canvas_snapshot)
    {
        lv_snapshot_free(ui_source_canvas_snapshot);
        ui_source_canvas_snapshot = NULL;
    }
    
    /*if(ui_source_canvas_buf)
    {
        esMEMS_Pfree(ui_dialog_canvas_buf, (LV_CANVAS_BUF_SIZE(640, 480, 32, LV_DRAW_BUF_STRIDE_ALIGN)+1023)>>10);
        ui_dialog_canvas_buf = NULL;
    }*/

    //eLIBs_printf("ui_canvas_blur_uninit esMEMS_FreeMemSize1=%d\n", esMEMS_FreeMemSize());
}
#endif




// 定义控件的操作函数集
static const ui_manger_ops_t g_source_ops = {
    .create = source_widget_create,
    .destroy = source_widget_destroy,
    .show = NULL,
    .hide = NULL,
    .handle_event = source_widget_handle_event,
};

// --- 公共函数 ---


static void fm_source_event_cb(radio_api_cmd_t message_cmd,void* payload)
{
    bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, message_cmd,payload, SRC_UNDEFINED, NULL);
}
static void ui_event_fm_set_up(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if((event_code == LV_EVENT_SHORT_CLICKED) || (event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}

static void ui_event_fm_add_preset(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		radio_set_preset_payload_t*radio_sta_payload=(radio_set_preset_payload_t*)infotainment_malloc(sizeof(radio_state_payload_t));
		
		if(g_radio_data_cache.preset_index!=0)
		{
			radio_sta_payload->preset_flag = 0;
			//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
			__msg("RADIO_CMD_SAVE_PRESET=%x\n",RADIO_CMD_SAVE_PRESET);
			fm_source_event_cb(RADIO_CMD_SAVE_PRESET,radio_sta_payload);
		}
		else
		{	
			radio_sta_payload->preset_flag = 1;
			//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
			__msg("RADIO_CMD_SAVE_PRESET=%x\n",RADIO_CMD_SAVE_PRESET);
			fm_source_event_cb(RADIO_CMD_SAVE_PRESET,radio_sta_payload);
		}
	}
	else if(event_code == LV_EVENT_SHORT_CLICKED)
	{
		//fm_source_event_cb()
		//bus_post_message(SRC_UI, ADDRESS_RADIO_SERVICE,MSG_PRIO_NORMAL, RADIO_CMD_SEEK_NEXT, NULL, NULL);	
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}


static void ui_event_fm_eq(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if((event_code == LV_EVENT_SHORT_CLICKED) || (event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		//fm_source_event_cb()
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}
static void ui_event_fm_auto_store(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		__msg("RADIO_CMD_AUTO_STORE=%x\n",RADIO_CMD_AUTO_STORE);
		
		fm_source_event_cb(RADIO_CMD_AUTO_STORE,NULL);
		//lv_label_set_text(g_source_state.radio_ps_name_label, "Auto Store");
	}
	else if((event_code == LV_EVENT_SHORT_CLICKED) || (event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		//fm_source_event_cb()
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}

static void ui_event_fm_list(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		//fm_source_event_cb(RADIO_CMD_AUTO_STORE);
		ui_tuner_show_open_win_by_type(&g_source_state,TUNER_LIST_WIN);
	}
	else if((event_code == LV_EVENT_SHORT_CLICKED) || (event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}



static void ui_event_fm_prev(lv_event_t * e)
	{
		ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
		lv_event_code_t event_code = lv_event_get_code(e);
		lv_obj_t * target = lv_event_get_target(e);
		lv_obj_t * current_target = lv_event_get_current_target(e);
		static __s8 last_event_code = -1;
	
		if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
		{
			return;
		}
	
		if(event_code == LV_EVENT_PRESSED)
		{
			//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
			last_event_code = LV_EVENT_PRESSED;
		}
		else if(event_code == LV_EVENT_RELEASED)
		{
			if(last_event_code==LV_EVENT_PRESSED)
			{
				__msg("RADIO_CMD_SEEK_PREV=%x\n",RADIO_CMD_SEEK_PREV);
				fm_source_event_cb(RADIO_CMD_PLAY_PREV,NULL);
				//lv_label_set_text(g_source_state.radio_ps_name_label, "Auto Seek");
			}
			last_event_code = -1;
		}
		else if((event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
		{
			fm_source_event_cb(RADIO_CMD_PLAY_PREV,NULL);
			//__msg("RADIO_CMD_TUNE_UP=%x\n",RADIO_CMD_TUNE_UP);
			//lv_label_set_text(g_source_state.radio_ps_name_label, "Manual Seek");
			last_event_code = event_code;
		}
		else if(event_code == LV_EVENT_SHORT_CLICKED)
		{
			last_event_code = event_code;
		}
		else if(event_code == LV_EVENT_PRESS_MOVEING)
			last_event_code = -1;
	}


static void ui_event_fm_switch_band(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;
	
	//__msg("current_target=%x,target=%x\n",current_target,target);
	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		__msg(" here return \n");
		return;
	}
	//__msg("event_code=%d,LV_EVENT_VALUE_CHANGED=%d\n",event_code,LV_EVENT_VALUE_CHANGED);
	if(event_code == LV_EVENT_VALUE_CHANGED)
	{
		//__msg("lv_obj_has_state(current_target, LV_STATE_CHECKED)=%d\n",lv_obj_has_state(current_target, LV_STATE_CHECKED));
		if(lv_obj_has_state(current_target, LV_STATE_CHECKED))
		{
			lv_obj_set_style_text_color(g_source_state.ui_fm_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(g_source_state.ui_am_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
			/*lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_AM_ICON_BG_BMP));
			//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
			ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);*/
			__msg("RADIO_CMD_SWITCH_AM_BAND=%x\n",RADIO_CMD_SWITCH_AM_BAND);
			fm_source_event_cb(RADIO_CMD_SWITCH_AM_BAND,NULL);
		}
		else
		{
			lv_obj_set_style_text_color(g_source_state.ui_fm_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(g_source_state.ui_am_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
			/*lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_FM_ICON_BG_BMP));
			//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
			ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);*/
			__msg("RADIO_CMD_SWITCH_FM_BAND=%x\n",RADIO_CMD_SWITCH_FM_BAND);
			fm_source_event_cb(RADIO_CMD_SWITCH_FM_BAND,NULL);
		}
	}
}

static void ui_event_fm_list_switch_band(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = 0;
	
	//__msg("current_target=%x,target=%x\n",current_target,target);
	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		__msg(" here return \n");
		return;
	}
	//__msg("event_code=%d,LV_EVENT_VALUE_CHANGED=%d\n",event_code,LV_EVENT_VALUE_CHANGED);
	if(event_code == LV_EVENT_VALUE_CHANGED)
	{
		//__msg("lv_obj_has_state(current_target, LV_STATE_CHECKED)=%d\n",lv_obj_has_state(current_target, LV_STATE_CHECKED));
		if(lv_obj_has_state(current_target, LV_STATE_CHECKED))
		{
			lv_obj_set_style_text_color(g_source_state.ui_list_fm_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(g_source_state.ui_list_am_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
			/*lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_AM_ICON_BG_BMP));
			//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
			ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);*/
			//__msg("RADIO_CMD_SWITCH_AM_BAND=%x\n",RADIO_CMD_SWITCH_AM_BAND);
			//fm_source_event_cb(RADIO_CMD_SWITCH_AM_BAND,NULL);
			__msg(" ui_event_fm_list_switch_band \n");
			ui_tuner_funtion_listbar_uninit(&g_source_state);
			ui_tuner_funtion_listbar_init(&g_source_state);
		}
		else
		{
			lv_obj_set_style_text_color(g_source_state.ui_list_fm_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_color(g_source_state.ui_list_am_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
			ui_tuner_funtion_listbar_uninit(&g_source_state);
			ui_tuner_funtion_listbar_init(&g_source_state);
			/*lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_FM_ICON_BG_BMP));
			//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
			ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);*/
			//__msg("RADIO_CMD_SWITCH_FM_BAND=%x\n",RADIO_CMD_SWITCH_FM_BAND);
			//fm_source_event_cb(RADIO_CMD_SWITCH_FM_BAND,NULL);
		}
	}
}

static void ui_event_fm_next(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = -1;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
		last_event_code = LV_EVENT_PRESSED;
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		if(last_event_code==LV_EVENT_PRESSED)
		{
			__msg("RADIO_CMD_SEEK_NEXT=%x\n",RADIO_CMD_SEEK_NEXT);
			fm_source_event_cb(RADIO_CMD_PLAY_NEXT,NULL);
			//lv_label_set_text(g_source_state.radio_ps_name_label, "Auto Seek");
		}
		last_event_code = -1;
	}
	else if((event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		__msg("RADIO_CMD_TUNE_DOWN=%x\n",RADIO_CMD_TUNE_DOWN);
		//fm_source_event_cb(RADIO_CMD_TUNE_DOWN,NULL);
		fm_source_event_cb(RADIO_CMD_PLAY_NEXT,NULL);
		//lv_label_set_text(g_source_state.radio_ps_name_label, "Manual Seek");
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_SHORT_CLICKED)
	{
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}



static void ui_event_fm_list_return(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = -1;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
		last_event_code = LV_EVENT_PRESSED;
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		if(last_event_code==LV_EVENT_PRESSED)
		{
			ui_tuner_show_open_win_by_type(&g_source_state,TUNER_PLAY_WIN);
		}
		last_event_code = -1;
	}
	else if((event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_SHORT_CLICKED)
	{
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}

static void ui_event_fm_list_screen(lv_event_t * e)
{
	__u32 i;
	//ui_fm_para_t * fm_para = lv_event_get_user_data(e);
	lv_event_code_t event_code = lv_event_get_code(e);
	lv_obj_t * target = lv_event_get_target(e);
	lv_obj_t * current_target = lv_event_get_current_target(e);
	static __u32 preset_index;
	static __s8 last_key_type = 0;	
	radio_set_preset_payload_t*radio_sta_payload=(radio_set_preset_payload_t*)infotainment_malloc(sizeof(radio_state_payload_t));
	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
		__msg("target=0x%x\n", target);
		
	}
	else if(event_code == LV_EVENT_SHORT_CLICKED)
	{
		if(last_key_type==-1)
		{
			last_key_type = 0;
			return;
		}
		//if(fm_para->switch_band)
		//	return;

		//__fm_msg("lv_obj_get_index(target)=%d\n", lv_obj_get_index(current_target));
		//ui_fm_ctrl_preset(fm_para, lv_obj_get_index(current_target));
		if(!lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED))
		{
			radio_sta_payload->preset_flag = 1;
			radio_sta_payload->preset_index = lv_obj_get_index(current_target);
			radio_sta_payload->band = 0;
			//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
			__msg("RADIO_CMD_SAVE_PRESET=%x\n",RADIO_CMD_SAVE_PRESET);
			fm_source_event_cb(RADIO_CMD_PLAY_PRESET,radio_sta_payload);
		}
		else 
		{
			radio_sta_payload->preset_flag = 1;
			radio_sta_payload->preset_index = lv_obj_get_index(current_target);
			radio_sta_payload->band = 1;
			fm_source_event_cb(RADIO_CMD_PLAY_PRESET,radio_sta_payload);
		}	
	}
	else if(event_code == LV_EVENT_LONG_PRESSED)
	{
		//if(fm_para->switch_band)
		//	return;

		//__fm_msg("lv_obj_get_index(target)=%d\n", lv_obj_get_index(current_target));
		//ui_fm_ctrl_long_preset(fm_para, lv_obj_get_index(current_target));
		if(!lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED))
		{
			radio_sta_payload->preset_flag = 1;
			radio_sta_payload->preset_index = lv_obj_get_index(current_target);
			radio_sta_payload->band = 0;
			//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
			__msg("RADIO_CMD_SAVE_PRESET=%x\n",RADIO_CMD_SAVE_PRESET);
			fm_source_event_cb(RADIO_CMD_SAVE_PRESET,radio_sta_payload);
		}
		else 
		{
			radio_sta_payload->preset_flag = 1;
			radio_sta_payload->preset_index = lv_obj_get_index(current_target);
			radio_sta_payload->band = 1;
			fm_source_event_cb(RADIO_CMD_SAVE_PRESET,radio_sta_payload);
		}	
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
	{
		last_key_type = -1;
	}
}

static void ui_event_fm_freq_panel(lv_event_t * e)
{
	ui_source_widget_state_t * source_widget_para = lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    lv_obj_t * current_target = lv_event_get_current_target(e);
	static __s8 last_event_code = -1;
    lv_coord_t scroll_x;

	if(lv_obj_has_flag(current_target, LV_OBJ_FLAG_HIDDEN))
	{
		return;
	}

	if(event_code == LV_EVENT_PRESSED)
	{
		//app_root_msg_send_opn(GUI_MSG_BEEP_SETTING,SEND_MCU_BEEP_SETTING_TIME,0,0,0,0,0);
		last_event_code = LV_EVENT_PRESSED;
	}
	else if(event_code == LV_EVENT_RELEASED)
	{
		if(last_event_code==LV_EVENT_PRESSED)
		{
		}
		last_event_code = -1;
	}
    else if(event_code == LV_EVENT_SCROLL)
	{
        scroll_x = lv_obj_get_scroll_x(current_target);
        __msg("scroll_x=%d\n", scroll_x);

        if(scroll_x < 0)
        {
            lv_obj_scroll_to_x(current_target, g_source_state.freq_scale_w - g_source_state.freq_panel_w - (g_source_state.freq_scale_setp_w / 2), LV_ANIM_OFF);
        }
        else if(scroll_x > (g_source_state.freq_scale_w - g_source_state.freq_panel_w))
        {
            lv_obj_scroll_to_x(current_target, (g_source_state.freq_scale_setp_w / 2), LV_ANIM_OFF);
        }
	}
	else if((event_code == LV_EVENT_LONG_PRESSED) || (event_code == LV_EVENT_LONG_PRESSED_REPEAT))
	{
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_SHORT_CLICKED)
	{
		last_event_code = event_code;
	}
	else if(event_code == LV_EVENT_PRESS_MOVEING)
		last_event_code = -1;
}




__s32 ui_source_timer(ui_source_create_config_t * source_para)
{

	return EPDK_OK;
}




static void ui_event_source_timer_cb(lv_timer_t * t)
{	
	ui_source_create_config_t * source_para = t->user_data;

#if 0//LV_F133_USE_ASYNC_THREAD
	memset(&clock_para->clock_msg, 0x00, sizeof(__gui_msg_t));
	clock_para->clock_msg.id = GUI_MSG_BACKCAR_OPN_TIMER;
	ui_clock_opn_msg(clock_para);
#else
	ui_source_timer(source_para);
#endif
}



void ui_source_widget_init(void) {
    // 注册控件
    ui_manager_register(MANGER_ID_SOURCE, &g_source_ops);
    printf("UI Source Widget: Registered with UI Manager.\n");
}


// --- 静态(内部)函数 ---


/**
 * @brief 创建 Source 控件 (由 ui_manager 调用)
 */
static lv_obj_t* source_widget_create(lv_obj_t* parent, void* arg) {
// arg 在我们的设计中可以被忽略，因为 initial_source 由 system service 控制
    // 但为了通用性，可以保留对 arg 的处理
    if (arg) {
        // 如果布局文件里也定义了 create_arg，可以让他覆盖 system service 的决定
        create_cfg = *(ui_source_create_config_t*)arg;
    }

    

   memset(&g_source_state, 0, sizeof(g_source_state));
    g_source_state.container = lv_obj_create(parent);
    lv_obj_remove_style_all(g_source_state.container);
   
   lv_obj_set_width(g_source_state.container, 390);
  lv_obj_set_height(g_source_state.container, 680);
  lv_obj_set_x(g_source_state.container, 0);
  lv_obj_set_y(g_source_state.container, 0);
  lv_obj_clear_flag(g_source_state.container, LV_OBJ_FLAG_SCROLLABLE);	   /// Flags
  lv_obj_set_style_radius(g_source_state.container, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(g_source_state.container, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(g_source_state.container, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(g_source_state.container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(g_source_state.container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(g_source_state.container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(g_source_state.container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(g_source_state.container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
	
	g_source_state.ui_source_bg_img = lv_img_create(g_source_state.container);
	   lv_obj_set_width( g_source_state.ui_source_bg_img, LV_SIZE_CONTENT);	/// 1
	   lv_obj_set_height( g_source_state.ui_source_bg_img, LV_SIZE_CONTENT);	  /// 1
	   lv_obj_set_align( g_source_state.ui_source_bg_img, LV_ALIGN_CENTER);
	   lv_obj_add_flag( g_source_state.ui_source_bg_img, LV_OBJ_FLAG_ADV_HITTEST);	 /// Flags
	   lv_obj_clear_flag( g_source_state.ui_source_bg_img, LV_OBJ_FLAG_SCROLLABLE);	   /// Flags
	   //lv_img_set_zoom( g_source_state.ui_source_bg_img, 1100);
	   
	g_source_state.ui_source_bg_panel = lv_obj_create(g_source_state.container);
	lv_obj_set_width(g_source_state.ui_source_bg_panel, 390);
	 lv_obj_set_height(g_source_state.ui_source_bg_panel, 680);
	 lv_obj_set_x(g_source_state.ui_source_bg_panel, 0);
	 lv_obj_set_y(g_source_state.ui_source_bg_panel, 0);
	 lv_obj_clear_flag(g_source_state.ui_source_bg_panel, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	 lv_obj_set_style_radius(g_source_state.ui_source_bg_panel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_bg_color(g_source_state.ui_source_bg_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_bg_opa(g_source_state.ui_source_bg_panel, 0xfffff, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_border_width(g_source_state.ui_source_bg_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_pad_left(g_source_state.ui_source_bg_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_pad_right(g_source_state.ui_source_bg_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_pad_top(g_source_state.ui_source_bg_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	 lv_obj_set_style_pad_bottom(g_source_state.ui_source_bg_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	




    /**
     * @brief 4. 【核心修改】根据音源类型，从对应的默认数据初始化运行时缓存
     */
    switch (create_cfg.initial_source) {
        case UI_SOURCE_TYPE_FM:
        case UI_SOURCE_TYPE_AM:
            // 从只读的默认值拷贝数据到可写的缓存中
            create_fm_am_panel(g_source_state.container);
            break;

        case UI_SOURCE_TYPE_DAB:
            create_dab_panel(g_source_state.container);
            break;

        case UI_SOURCE_TYPE_BT_MUSIC:
            create_bt_panel(g_source_state.container);
            break;

        case UI_SOURCE_TYPE_USB_MUSIC:
            create_usb_panel(g_source_state.container);
            break;

        default:
            // ... (与上一版相同) ...
            break;
    }

    return g_source_state.container;
}
/**
 * @brief 销毁控件
 */
static void source_widget_destroy(ui_manger_t* manger) {
    if (manger && manger->instance) {
        lv_obj_del(manger->instance);
    }
	ui_source_uninit_res(&g_source_state);
    memset(&g_source_state, 0, sizeof(g_source_state));
}


static void refresh_radio_ui_from_cache(void) {
    if (!g_source_state.container || !g_source_state.radio_panel) return;

    char freq_buf[32];
	char pty_data[32];
     if(g_radio_data_cache.band==0)
        snprintf(freq_buf, sizeof(freq_buf), "%.1f MHz", (float)g_radio_data_cache.frequency / 1000.0f);
    else
        snprintf(freq_buf, sizeof(freq_buf), "%d KHz", g_radio_data_cache.frequency);
    
    lv_label_set_text(g_source_state.radio_freq_label, freq_buf);
	//printf("freq_buf=%s.\n",freq_buf);
    //lv_label_set_text(g_source_state.radio_band_label, g_radio_data_cache.band_str);
    lv_label_set_text(g_source_state.radio_ps_name_label, g_radio_data_cache.ps_name);
	eLIBs_strcpy(pty_data,fm_res_rds_pty_string[g_radio_data_cache.pty]);
    lv_label_set_text(g_source_state.radio_pty_name_label, pty_data);
	//printf("pty_name=%s.\n",g_radio_data_cache.pty_name);
}

static void ui_tuner_clear_freq_scale(void)
{
	for(__u32 i=0;i<g_source_state.frq_cnt;i++)
	{
		lv_obj_del(g_source_state.ui_freq_text[i]);
		g_source_state.ui_freq_text[i] = NULL;
	}
	g_source_state.frq_cnt = 0;
}

static void ui_tuner_update_freq_scale(void)
{
	__s32 freq_scale_start_x;
	if(g_radio_data_cache.band==0)
	{
        freq_scale_start_x = g_source_state.freq_scale_setp_w * 4 + ((g_radio_data_cache.frequency-g_radio_data_cache.min_freq)/100) * (g_source_state.freq_scale_setp_w / 5) - (g_source_state.freq_panel_w / 2);
	}
	else
		freq_scale_start_x = 0;

    __msg("freq_scale_start_x=%d\n",freq_scale_start_x);
    lv_obj_scroll_to_x(g_source_state.ui_freq_panel, freq_scale_start_x,LV_ANIM_OFF);
}
static void ui_tuner_update_freq(void)
{
	
	char buf[16];
	 if(g_radio_data_cache.band==0)
	 {
		 snprintf(buf, sizeof(buf), "%.1f MHz", (float)g_radio_data_cache.frequency / 1000.0f);
   			 lv_label_set_text(g_source_state.radio_freq_label, buf);
		
	 }
	else
	{
		 snprintf(buf, sizeof(buf), "%d KHz", g_radio_data_cache.frequency);
   			 lv_label_set_text(g_source_state.radio_freq_label, buf);
			 
	 }
}
static void ui_tuner_update_ps_pty(__u8 update)
{
	if(update)
	{
		char pty_data[32]={0};
		eLIBs_strcpy(pty_data,fm_res_rds_pty_string[g_radio_data_cache.pty]);
	        lv_label_set_text(g_source_state.radio_ps_name_label, g_radio_data_cache.ps_name);
	        lv_label_set_text(g_source_state.radio_pty_name_label,pty_data);
	}
	else
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, " ");
        	lv_label_set_text(g_source_state.radio_pty_name_label," ");
	}
}

static void ui_tuner_update_perset(void)
{
	
	char preset[16]={0};
	__msg("g_radio_data_cache.preset_index=%d\n",g_radio_data_cache.preset_index);
	 if(g_radio_data_cache.preset_index==0)
	 {
		lv_imgbtn_set_state(g_source_state.ui_add_preset_button, LV_IMGBTN_STATE_RELEASED);
		lv_obj_add_flag(g_source_state.ui_add_preset_num, LV_OBJ_FLAG_HIDDEN);
	 }
	else
	{
		lv_imgbtn_set_state(g_source_state.ui_add_preset_button, LV_IMGBTN_STATE_CHECKED_RELEASED);
		lv_obj_clear_flag(g_source_state.ui_add_preset_num, LV_OBJ_FLAG_HIDDEN);
		sprintf(preset,"%d",g_radio_data_cache.preset_index);
		lv_label_set_text(g_source_state.ui_add_preset_num, preset);	 
	 }
}

static void ui_tuner_update_seek(radio_cur_seeking_type_e seek_type)
{
	if(seek_type == CUR_AUTO_SEEK)
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, "Auto Seek");
	}
	else if(seek_type == CUR_MANUAL_SEEK)
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, "Manual Seek");
	}
	else if(seek_type == CUR_PERSET_SEEK)
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, "Preset Seek");
	}
	else if(seek_type == CUR_PTY_SEEK)
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, "PTY Seek");
	}
	else if(seek_type == CUR_AUTOSTORE_SEEK)
	{
		lv_label_set_text(g_source_state.radio_ps_name_label, "Auto Store");
	}
	else
		lv_label_set_text(g_source_state.radio_ps_name_label, " ");
}



static void ui_tuner_update_band(void) {
	char freq_buf[32]={0};
	__s32 i,step_cnt;
	__s32 freq_scale_start_x,freq_scale_width;
	__msg("g_radio_data_cache.max_freq=%d\n",g_radio_data_cache.max_freq);
	__msg("g_radio_data_cache.min_freq=%d\n",g_radio_data_cache.min_freq);
	__msg("g_radio_data_cache.tu_step=%d\n",g_radio_data_cache.tu_step);
	

    ui_tuner_update_freq_scale();
	ui_tuner_update_freq();

	
	if(g_radio_data_cache.band==1)
	 {
		 lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_AM_ICON_BG_BMP));
		//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
		ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);
	 }
	 else
 	{
 		lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_FM_ICON_BG_BMP));
		//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
		ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);
 	}

}



//P1    P2    P3   P4   P5    P6
void __tuner_list_update_preset(ui_source_widget_state_t * source_para, __u8 update)
{	
	__u8 j=0;
	__u8 str[256];
	lv_obj_t * ui_list_item;
	lv_obj_t * ui_list_item_panel;
	lv_obj_t * ui_list_item_text;
	lv_obj_t * ui_list_item_index;
	//reg_fm_para_t* para;
	
	//para = (reg_fm_para_t*)dsk_reg_get_para_by_app(REG_APP_FM);

	if(update)
	{
#ifdef NO_FM3_AM2_BAND		
		for(j=0;j<15;j++)
#else
		for(j=0;j<6;j++)
#endif
		{
			ui_list_item = lv_obj_get_child(source_para->ui_list_para.item_obj, j);
			ui_list_item_panel = lv_obj_get_child(ui_list_item, 0);
			ui_list_item_text = lv_obj_get_child(ui_list_item_panel, 0);
			ui_list_item_index = lv_obj_get_child(ui_list_item_panel, 1);

			//__fm_msg("curFM1_3AM1_2_index=%d, j=%d, fmchannelfreq=%d, FM1_3_AM1_2_freq=%d, curFM1_3AM1_2_id=%d\n", fm_para->curFM1_3AM1_2_index, j, fm_para->fmchannelfreq, para->FM1_3_AM1_2_freq[fm_para->curFM1_3AM1_2_id][j], fm_para->curFM1_3AM1_2_id);

			if(!lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED))
			{
				if((g_radio_data_cache.preset_index == j) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[0][j]))
				{
					if(!lv_obj_has_state(ui_list_item_panel, LV_STATE_CHECKED))
					{
						__msg("ui_list_item_panel=0x%x\n", ui_list_item_panel);
						lv_obj_add_state(ui_list_item_panel, LV_STATE_CHECKED);
					}
					lv_obj_set_style_text_color(ui_list_item_text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_text_color(ui_list_item_index, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
				}
				else
				{
					if(lv_obj_has_state(ui_list_item_panel, LV_STATE_CHECKED))
					{
						
						lv_obj_clear_state(ui_list_item_panel, LV_STATE_CHECKED);
					}
					lv_obj_set_style_text_color(ui_list_item_text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_text_color(ui_list_item_index, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
				}
				  
				
					sprintf(str, "%.1f					MHz", (float)g_radio_data_cache.FM1_3_AM1_2_freq[0][j]/1000.0f);
				
			}
			else
			{
				if((g_radio_data_cache.preset_index == j) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[1][j]))
				{
					if(!lv_obj_has_state(ui_list_item_panel, LV_STATE_CHECKED))
					{
						__msg("ui_list_item_panel=0x%x\n", ui_list_item_panel);
						lv_obj_add_state(ui_list_item_panel, LV_STATE_CHECKED);
					}
					lv_obj_set_style_text_color(ui_list_item_text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_text_color(ui_list_item_index, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
				}
				else
				{
					if(lv_obj_has_state(ui_list_item_panel, LV_STATE_CHECKED))
					{
						
						lv_obj_clear_state(ui_list_item_panel, LV_STATE_CHECKED);
					}
					lv_obj_set_style_text_color(ui_list_item_text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
					lv_obj_set_style_text_color(ui_list_item_index, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
				}
				  
				
					sprintf(str, "%d                  KHz", g_radio_data_cache.FM1_3_AM1_2_freq[1][j]);
				
			}	
			
			
			lv_label_set_text(ui_list_item_text, str);
		}
	}
}


static __s32 fm_draw_menu_item_text(__lv_draw_para_t *draw_param, lv_obj_t * text, __u8 index)
{
	__u8 str[256];
	int ret;
	__u8 j=0,i=0;
	__u8 ucStringBuf[GUI_NAME_MAX + 1]={0};
	ui_source_widget_state_t * source_para;

	source_para =(ui_source_widget_state_t *)draw_param->attr;		

	if(index == 0)
	{
		sprintf(str, "%02d", draw_param->index + 1);
		//__msg("str = %s\n",str);
		if(!lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED))
		{
			if((g_radio_data_cache.preset_index == draw_param->index) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[0][draw_param->index]))
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
			else
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
		}
		else
		{
			if((g_radio_data_cache.preset_index == draw_param->index) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[1][draw_param->index]))
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
			else
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
		}

		lv_label_set_text(text, str);
	}
	else if(index == 1)
	{
		//__msg("curFM1_3AM1_2_id = %d %d\n",fm_para->curFM1_3AM1_2_id);
		//__msg("draw_param->index = %d\n",draw_param->index);
		
		for(int i = 0;i< draw_param->index;i++)
		{
			//__msg("draw_param->index = %d\n", para->FM1_3_AM1_2_freq[fm_para->curFM1_3AM1_2_id][i]);

		}
		__msg("lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED) = %d\n",lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED));
		if(!lv_obj_has_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED))
		{
			sprintf(str, "%.1f                  MHz", (float)g_radio_data_cache.FM1_3_AM1_2_freq[0][draw_param->index]/1000.0f);
			if((g_radio_data_cache.preset_index == draw_param->index) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[0][draw_param->index]))
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
			else
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
		}
		else
		{
			sprintf(str, "%d                  KHz", g_radio_data_cache.FM1_3_AM1_2_freq[1][draw_param->index]);
			if((g_radio_data_cache.preset_index == draw_param->index) && (g_radio_data_cache.frequency == g_radio_data_cache.FM1_3_AM1_2_freq[1][draw_param->index]))
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0x00ff00), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
			else
			{
				lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
			}
		}

		

		__msg("str = %s\n",str);
		lv_label_set_text(text, str);
	}
}






static __s32 fm_view_draw_listview_item_text(__lv_draw_para_t *draw_param, lv_obj_t * text, __u8 index)
{
	__s32 ret = 0;
	ui_source_widget_state_t * source_para;
	
	source_para = (ui_source_widget_state_t *)draw_param->attr;				//for draw the picture  in different media type

	switch(source_para->cur_tuner_win)
	{

		case TUNER_LIST_WIN:
		{
			ret = fm_draw_menu_item_text(draw_param, text, index);
		}
		break;
	}

	return EPDK_OK;	
}

static __s32 fm_draw_listview_item(__lv_draw_para_t *draw_param)
{
	__s32 ret;
	
	ret = 2;
	
	return ret;	
}


static __s32 fm_view_draw_listview_item(__lv_draw_para_t *draw_param)
{
	__s32 ret = 0;
	ui_source_widget_state_t * source_para;
	
	source_para = (ui_source_widget_state_t *)draw_param->attr;				//for draw the picture  in different media type

	switch(source_para->cur_tuner_win)
	{

		case TUNER_LIST_WIN:
		{
			 ret = fm_draw_listview_item(draw_param);
		}
		break;
	}

	return ret;	
}



static void * ui_fm_draw_item(__lv_draw_para_t *draw_param)
{	
	__s32 ret;
	ui_source_widget_state_t * source_para = (ui_source_widget_state_t *)draw_param->attr;


	lv_obj_t * ui_list_item = lv_obj_create(source_para->ui_list_para.item_obj);
    lv_obj_remove_style_all(ui_list_item);
	lv_obj_set_width(ui_list_item, source_para->ui_list_para.item_width);
	lv_obj_set_height(ui_list_item, source_para->ui_list_para.item_height);
	lv_obj_set_x(ui_list_item, 0);
	lv_obj_clear_flag(ui_list_item, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	lv_obj_set_style_radius(ui_list_item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui_list_item, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	//lv_obj_set_style_bg_img_src(ui_list_item, ui_fm_get_res(fm_para, FM_LONG_STR_SCROLL_BG_BMP), LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_t * ui_list_item_panel = lv_obj_create(ui_list_item);
    lv_obj_remove_style_all(ui_list_item_panel);
    lv_obj_set_x(ui_list_item_panel, 0);
    lv_obj_set_height(ui_list_item_panel, lv_pct(92));
    lv_obj_set_width(ui_list_item_panel, lv_pct(100));
	//lv_obj_set_align(ui_list_item_panel, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_list_item_panel, LV_OBJ_FLAG_CHECKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);      /// Flags
    lv_obj_clear_flag(ui_list_item_panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_opa(ui_list_item_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_list_item_panel, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_list_item_panel, 255, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_radius(ui_list_item_panel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui_list_item_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui_list_item_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui_list_item_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui_list_item_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);


	
	lv_obj_t * ui_list_item_text = lv_label_create(ui_list_item_panel);
	lv_obj_set_width(ui_list_item_text, lv_pct(80));//lv_pct(50)s
	lv_obj_set_height(ui_list_item_text, LV_SIZE_CONTENT);	  /// 0
	lv_obj_set_x(ui_list_item_text, 70);
	lv_obj_set_y(ui_list_item_text, 0);
	lv_obj_set_align(ui_list_item_text, LV_ALIGN_LEFT_MID);
	lv_label_set_long_mode(ui_list_item_text, LV_LABEL_LONG_CLIP);
	lv_obj_set_style_text_color(ui_list_item_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(ui_list_item_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui_list_item_text, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui_list_item_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
	fm_view_draw_listview_item_text(draw_param, ui_list_item_text, 1);	

	ret = fm_view_draw_listview_item(draw_param);
	/*if(ret == 1)
	{
		lv_obj_clear_flag(ui_list_item, LV_OBJ_FLAG_CLICKABLE);	  /// Flags
		
		lv_obj_t * ui_list_item_arrow = lv_obj_create(ui_list_item_panel);
		lv_obj_remove_style_all(ui_list_item_arrow);
		lv_obj_set_width(ui_list_item_arrow, 225);//lv_pct(45)
		lv_obj_set_height(ui_list_item_arrow, lv_pct(100));
		lv_obj_set_x(ui_list_item_arrow, 325);//lv_pct(51)
		lv_obj_set_y(ui_list_item_arrow, lv_pct(0));
		lv_obj_set_flex_flow(ui_list_item_arrow, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(ui_list_item_arrow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_clear_flag(ui_list_item_arrow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);		/// Flags
		lv_obj_add_flag(ui_list_item_arrow, LV_OBJ_FLAG_ADV_HITTEST);	/// Flags
		lv_obj_set_style_bg_opa(ui_list_item_arrow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

		lv_obj_t * ui_list_item_left_arrow = lv_obj_create(ui_list_item_arrow);
		lv_obj_remove_style_all(ui_list_item_left_arrow);
		lv_obj_set_width(ui_list_item_left_arrow, 41);
		lv_obj_set_height(ui_list_item_left_arrow, 47);
		lv_obj_set_align(ui_list_item_left_arrow, LV_ALIGN_CENTER);
		lv_obj_add_flag(ui_list_item_left_arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE); 	 /// Flags
		lv_obj_clear_flag(ui_list_item_left_arrow, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
		//lv_obj_set_style_bg_img_src(ui_list_item_left_arrow, ui_fm_get_res(fm_para, FM_LEFT_ARROW_SET_BMP), LV_PART_MAIN | LV_STATE_DEFAULT);
		//lv_obj_set_style_bg_img_src(ui_list_item_left_arrow, ui_fm_get_res(fm_para, FM_LEFT_ARROW_SET_P_BMP), LV_PART_MAIN | LV_STATE_PRESSED);

		lv_obj_t * ui_list_item_arrow_text = lv_label_create(ui_list_item_arrow);
		lv_obj_set_width(ui_list_item_arrow_text, 143);//lv_pct(50)//183
		lv_obj_set_height(ui_list_item_arrow_text, LV_SIZE_CONTENT);	/// 0
		lv_obj_set_align(ui_list_item_arrow_text, LV_ALIGN_LEFT_MID);
		lv_label_set_long_mode(ui_list_item_arrow_text, LV_LABEL_LONG_CLIP);
		lv_obj_set_style_text_color(ui_list_item_arrow_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_opa(ui_list_item_arrow_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(ui_list_item_arrow_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_font(ui_list_item_arrow_text, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);
		fm_view_draw_listview_item_text(draw_param, ui_list_item_arrow_text, 0);
		
		lv_obj_t * ui_list_item_right_arrow = lv_obj_create(ui_list_item_arrow);
		lv_obj_remove_style_all(ui_list_item_right_arrow);
		lv_obj_set_width(ui_list_item_right_arrow, 41);
		lv_obj_set_height(ui_list_item_right_arrow, 47);
		lv_obj_set_align(ui_list_item_right_arrow, LV_ALIGN_CENTER);
		lv_obj_add_flag(ui_list_item_right_arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE); 	 /// Flags
		lv_obj_clear_flag(ui_list_item_right_arrow, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
		//lv_obj_set_style_bg_img_src(ui_list_item_right_arrow, ui_fm_get_res(fm_para, FM_RIGHT_ARROW_SET_BMP), LV_PART_MAIN | LV_STATE_DEFAULT);
		//lv_obj_set_style_bg_img_src(ui_list_item_right_arrow, ui_fm_get_res(fm_para, FM_RIGHT_ARROW_SET_P_BMP), LV_PART_MAIN | LV_STATE_PRESSED);
		
		lv_obj_add_event_cb(ui_list_item_arrow, ui_event_fm_menu_arrow_screen, LV_EVENT_ALL, source_para);
	}
	else if(ret == 0)
	{
		lv_obj_set_width(ui_list_item_text, lv_pct(60));
		
		lv_obj_t * ui_list_item_arrow = lv_obj_create(ui_list_item_panel);
		lv_obj_remove_style_all(ui_list_item_arrow);
		lv_obj_set_width(ui_list_item_arrow, 225);//lv_pct(45)
		lv_obj_set_height(ui_list_item_arrow, lv_pct(100));
		lv_obj_set_x(ui_list_item_arrow, 308);//lv_pct(51)
		lv_obj_set_y(ui_list_item_arrow, lv_pct(0));
		lv_obj_set_flex_flow(ui_list_item_arrow, LV_FLEX_FLOW_ROW);
		lv_obj_set_flex_align(ui_list_item_arrow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
		lv_obj_clear_flag(ui_list_item_arrow, LV_OBJ_FLAG_SCROLLABLE);		/// Flags
		lv_obj_add_flag(ui_list_item_arrow, LV_OBJ_FLAG_ADV_HITTEST | LV_OBJ_FLAG_EVENT_BUBBLE);	/// Flags
		lv_obj_set_style_bg_opa(ui_list_item_arrow, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
		
		__msg("FM_UN_ENTER_ICON_BMP\n");
	    lv_obj_t * ui_list_item_un_enter = lv_imgbtn_create(ui_list_item_arrow);
	    //lv_imgbtn_set_src(ui_list_item_un_enter, LV_IMGBTN_STATE_RELEASED, NULL, ui_fm_get_res(fm_para, FM_UN_ENTER_ICON_BMP), NULL);
	    //lv_imgbtn_set_src(ui_list_item_un_enter, LV_IMGBTN_STATE_PRESSED, NULL, ui_fm_get_res(fm_para, FM_ENTER_ICON_BMP), NULL);
	    lv_obj_set_height(ui_list_item_un_enter, 70);
	    lv_obj_set_width(ui_list_item_un_enter, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_align(ui_list_item_un_enter, LV_ALIGN_CENTER);
	    lv_obj_add_flag(ui_list_item_un_enter, LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags
	}
	else if(ret == 2)*/
	{		
		lv_obj_t * ui_list_item_index = lv_label_create(ui_list_item_panel);
		lv_obj_set_width(ui_list_item_index, 30);//lv_pct(50)
		lv_obj_set_height(ui_list_item_index, LV_SIZE_CONTENT);	  /// 0
		lv_obj_set_x(ui_list_item_index, 0);
		lv_obj_set_y(ui_list_item_index, 0);
		lv_obj_set_align(ui_list_item_index, LV_ALIGN_LEFT_MID);
		lv_label_set_long_mode(ui_list_item_index, LV_LABEL_LONG_CLIP);
		lv_obj_set_style_text_color(ui_list_item_index, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_opa(ui_list_item_index, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_font(ui_list_item_index, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_align(ui_list_item_index, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
		fm_view_draw_listview_item_text(draw_param, ui_list_item_index, 0);
	}

	lv_obj_t * ui_list_item_line = lv_obj_create(ui_list_item);
    lv_obj_remove_style_all(ui_list_item_line);
	lv_obj_set_width(ui_list_item_line, lv_pct(100));
	lv_obj_set_height(ui_list_item_line, 2);
	lv_obj_set_align(ui_list_item_line, LV_ALIGN_BOTTOM_MID);
	lv_obj_clear_flag(ui_list_item_line, LV_OBJ_FLAG_SCROLLABLE);	  /// Flags
	lv_obj_set_style_bg_color(ui_list_item_line, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui_list_item_line, 50, LV_PART_MAIN | LV_STATE_DEFAULT);

	//__msg("ui_list_item=0x%x, draw_param->index=%d\n", ui_list_item, draw_param->index);
	lv_obj_add_event_cb(ui_list_item, ui_event_fm_list_screen, LV_EVENT_ALL, source_para);

	return (void *)ui_list_item;
}


static void ui_tuner_funtion_listbar_init(ui_source_widget_state_t *source_para)
{
	if(!source_para->ui_list_para.item_obj)
	{
	    source_para->ui_list_para.list_attr = (void *)source_para;
		source_para->ui_list_para.lbar_draw = (__lv_draw_item)ui_fm_draw_item;
		
		//__msg("ui_fm_draw_item=0x%x, fm_para->ui_list_para.menu_obj=0x%x\n", ui_fm_draw_item, fm_para->ui_list_para.menu_obj);
			
		source_para->fm_menu_obj = ui_menu_create(&source_para->ui_list_para, NULL);
		/*if(fm_para->ui_list_para.top_browse_obj)
		{
			__msg("fm_view_funtion_listbar_init top_browse_obj = 0x%x\n", fm_para->ui_list_para.top_browse_obj);
			lv_obj_add_event_cb(fm_para->ui_list_para.top_browse_obj, ui_event_fm_fmband_screen, LV_EVENT_ALL, fm_para);			
			
			lv_obj_t * ui_menu_top_browse_fm_label = lv_label_create(fm_para->ui_list_para.top_browse_obj);
			lv_obj_set_width(ui_menu_top_browse_fm_label, 50);	 /// 1
			lv_obj_set_height(ui_menu_top_browse_fm_label, 50);    /// 1
			lv_obj_set_align(ui_menu_top_browse_fm_label, LV_ALIGN_CENTER);
			lv_label_set_text(ui_menu_top_browse_fm_label, "FM");
			lv_obj_set_style_text_color(ui_menu_top_browse_fm_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_opa(ui_menu_top_browse_fm_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
			lv_obj_set_style_text_font(ui_menu_top_browse_fm_label, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);
			if((fm_para->curFM1_3AM1_2_id<dMAX_BAND))
				lv_obj_add_state(fm_para->ui_list_para.top_browse_obj, LV_STATE_CHECKED);
		}
		
		if(fm_para->ui_list_para.mid_browse_obj)
		{
			__msg("fm_view_funtion_listbar_init mid_browse_obj = 0x%x\n", fm_para->ui_list_para.mid_browse_obj);
			lv_obj_add_event_cb(fm_para->ui_list_para.mid_browse_obj, ui_event_fm_amband_screen, LV_EVENT_ALL, fm_para);

			lv_obj_t * ui_menu_mid_browse_am_label = lv_label_create(fm_para->ui_list_para.mid_browse_obj);
		    lv_obj_set_width(ui_menu_mid_browse_am_label, 50);   /// 1
		    lv_obj_set_height(ui_menu_mid_browse_am_label, 50);    /// 1
		    lv_obj_set_align(ui_menu_mid_browse_am_label, LV_ALIGN_CENTER);
		    lv_label_set_text(ui_menu_mid_browse_am_label, "AM");
		    lv_obj_set_style_text_color(ui_menu_mid_browse_am_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
		    lv_obj_set_style_text_opa(ui_menu_mid_browse_am_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		    lv_obj_set_style_text_font(ui_menu_mid_browse_am_label, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);
			if((fm_para->curFM1_3AM1_2_id>=dMAX_BAND))
				lv_obj_add_state(fm_para->ui_list_para.mid_browse_obj, LV_STATE_CHECKED);
		}*/
	}
	__msg("fm_view_funtion_listbar_init end!!!\n");
}

static void ui_tuner_funtion_listbar_uninit(ui_source_widget_state_t *source_para)
{
	if(source_para->ui_list_para.item_obj != NULL)
	{
		__msg("fm_view_funtion_listbar_uninit\n");
		lv_obj_clean(source_para->ui_list_para.item_obj);//删除父对象下的子对象，不删除本身
		source_para->ui_list_para.item_obj = NULL;
		//lv_obj_clean(source_para->ui_list_para.mid_browse_obj);//删除父对象下的子对象，不删除本身
		//source_para->ui_list_para.mid_browse_obj = NULL;
		ui_menu_destroy(source_para->fm_menu_obj);
	}
}




static void create_fm_am_preset_list_win(lv_obj_t* parent)
{
	g_source_state.cur_tuner_win = TUNER_LIST_WIN;
	

	
	g_source_state.ui_list_para.menu_obj = g_source_state.radio_list;
	
	g_source_state.ui_list_para.item_cnt = 15;
	
	ui_tuner_funtion_listbar_init(&g_source_state);
	//lv_obj_add_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_clear_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_add_event_cb(g_source_state.radio_list_back_button, ui_event_fm_list_return, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.radio_list_band_button, ui_event_fm_list_switch_band,LV_EVENT_ALL, &g_source_state);
	//lv_obj_add_event_cb(g_source_state.radio_list_band_button, ui_event_fm_list_switch_band,LV_EVENT_ALL, &g_source_state);
	
}

static void close_fm_am_preset_list_win(void)
{

	
	ui_tuner_funtion_listbar_uninit(&g_source_state);
	//lv_obj_clear_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_add_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_remove_event_cb(g_source_state.radio_list_band_button, ui_event_fm_list_switch_band);
	lv_obj_remove_event_cb(g_source_state.radio_list_back_button, ui_event_fm_list_return);
	//lv_obj_clean(source_para->ui_list_para.top_browse_obj);//删除父对象下的子对象，不删除本身
	//source_para->ui_list_para.top_browse_obj = NULL;
	
}


static void create_fm_am_play_win(lv_obj_t* parent)
{
	g_source_state.cur_tuner_win = TUNER_PLAY_WIN;
	
	//lv_obj_add_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_clear_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_add_event_cb(g_source_state.ui_source_setup, ui_event_fm_set_up, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_add_preset_button, ui_event_fm_add_preset, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_tuner_eq_button, ui_event_fm_eq, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_auto_store_button, ui_event_fm_auto_store, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_tuner_list_button, ui_event_fm_list, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_prev_button, ui_event_fm_prev, LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_band_button, ui_event_fm_switch_band,LV_EVENT_ALL, &g_source_state);
	lv_obj_add_event_cb(g_source_state.ui_next_button, ui_event_fm_next, LV_EVENT_ALL, &g_source_state);
}

static void close_fm_am_play_win(void)
{

	
	//lv_obj_clear_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_add_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags

	lv_obj_remove_event_cb(g_source_state.ui_source_setup, ui_event_fm_set_up);
	lv_obj_remove_event_cb(g_source_state.ui_add_preset_button, ui_event_fm_add_preset);
	lv_obj_remove_event_cb(g_source_state.ui_tuner_eq_button, ui_event_fm_eq);
	lv_obj_remove_event_cb(g_source_state.ui_auto_store_button, ui_event_fm_auto_store);
	lv_obj_remove_event_cb(g_source_state.ui_tuner_list_button, ui_event_fm_list);
	lv_obj_remove_event_cb(g_source_state.ui_prev_button, ui_event_fm_prev);
	lv_obj_remove_event_cb(g_source_state.ui_band_button, ui_event_fm_switch_band);
	lv_obj_remove_event_cb(g_source_state.ui_next_button, ui_event_fm_next);
	
}





static void ui_tuner_show_open_win_by_type(ui_source_widget_state_t *source_para, __u8 bwin_type)
{
	
	
	if(source_para==NULL)
		return;
		
	if(source_para->cur_tuner_win== bwin_type)
		return;


	//close other win
	switch(source_para->cur_tuner_win)
	{
		case TUNER_PLAY_WIN:	
			close_fm_am_play_win();			
			break;

		case TUNER_LIST_WIN:
			close_fm_am_preset_list_win();
			break;
	}
	//open win by type
	switch(bwin_type)
	{
		case TUNER_PLAY_WIN:
			create_fm_am_play_win(source_para);

			break;

		case TUNER_LIST_WIN:
			create_fm_am_preset_list_win(source_para);
			break;
			
	}
}



/**
 * @brief 创建 FM/AM 播放器界面
 */
static void create_fm_am_panel(lv_obj_t* parent) {

	
    char freq_buf[32];
	char pty_data[32];
	__u32 step_cnt=0;
	__s32 freq_scale_start_x=0;
	__u32 freq_scale_width = 0;
	__s16 i,j = 0;
char preset[3];
	g_source_state.cur_band  = g_radio_data_cache.band;
	g_source_state.cur_tuner_win = TUNER_PLAY_WIN;
	g_source_state.radio_play_panel = lv_obj_create(parent);
	 lv_obj_set_height(g_source_state.radio_play_panel, lv_pct(100));
	   lv_obj_set_width(g_source_state.radio_play_panel, lv_pct(100));
	   lv_obj_set_x(g_source_state.radio_play_panel, 0);
	   lv_obj_set_y(g_source_state.radio_play_panel, 0);
	   lv_obj_clear_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	lv_obj_add_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	 lv_obj_set_style_radius(g_source_state.radio_play_panel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.radio_play_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.radio_play_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
    g_source_state.radio_panel = lv_obj_create(g_source_state.radio_play_panel);
    lv_obj_set_width(g_source_state.radio_panel, 390);
    lv_obj_set_height(g_source_state.radio_panel, 555);
    lv_obj_set_x(g_source_state.radio_panel, 0);
    lv_obj_set_y(g_source_state.radio_panel, 0);
    lv_obj_clear_flag(g_source_state.radio_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.radio_panel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.radio_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.radio_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	 
	   

	g_source_state.ui_tuner_set_pancel = lv_obj_create(g_source_state.radio_panel);
    lv_obj_set_height(g_source_state.ui_tuner_set_pancel, 50);
    lv_obj_set_width(g_source_state.ui_tuner_set_pancel, lv_pct(100));
    lv_obj_set_x(g_source_state.ui_tuner_set_pancel, 0);
    lv_obj_set_y(g_source_state.ui_tuner_set_pancel, 280);
    lv_obj_set_flex_flow(g_source_state.ui_tuner_set_pancel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_source_state.ui_tuner_set_pancel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_tuner_set_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_tuner_set_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_tuner_small_icon = lv_img_create(g_source_state.radio_panel);
    lv_img_set_src(g_source_state.ui_tuner_small_icon, ui_source_get_res(&g_source_state,TUNER_SMALL_ICON_BMP));
    lv_obj_set_width(g_source_state.ui_tuner_small_icon, 42);
    lv_obj_set_height(g_source_state.ui_tuner_small_icon, 42);
    lv_obj_set_x(g_source_state.ui_tuner_small_icon, 22);
    lv_obj_set_y(g_source_state.ui_tuner_small_icon, 22);
    lv_obj_add_flag(g_source_state.ui_tuner_small_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_source_state.ui_tuner_small_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    g_source_state.ui_tuner_text = lv_label_create(g_source_state.radio_panel);
    lv_obj_set_width(g_source_state.ui_tuner_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_tuner_text, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_tuner_text, 66);
    lv_obj_set_y(g_source_state.ui_tuner_text, 34);
    lv_label_set_text(g_source_state.ui_tuner_text, "Tuner");
    lv_obj_set_style_text_color(g_source_state.ui_tuner_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.ui_tuner_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.ui_tuner_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_tuner_big_icon_bg = lv_img_create(g_source_state.radio_panel);
	 __msg("g_radio_data_cache.band=%d\n",g_radio_data_cache.band);
	 if(g_radio_data_cache.band==0)
   	 	lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_FM_ICON_BG_BMP));
	else
		lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_AM_ICON_BG_BMP));
    lv_obj_set_width(g_source_state.ui_tuner_big_icon_bg, 157);
    lv_obj_set_height(g_source_state.ui_tuner_big_icon_bg, 157);
    lv_obj_set_x(g_source_state.ui_tuner_big_icon_bg, 9);
    lv_obj_set_y(g_source_state.ui_tuner_big_icon_bg, 86);
    lv_obj_add_flag(g_source_state.ui_tuner_big_icon_bg, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_source_state.ui_tuner_big_icon_bg, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    g_source_state.ui_tuner_icon = lv_img_create(g_source_state.radio_panel);
    lv_img_set_src(g_source_state.ui_tuner_icon, ui_source_get_res(&g_source_state,TUNER_ICON_BMP));
    lv_obj_set_width(g_source_state.ui_tuner_icon, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_tuner_icon, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_tuner_icon, 54);
    lv_obj_set_y(g_source_state.ui_tuner_icon, 129);
    lv_obj_add_flag(g_source_state.ui_tuner_icon, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_source_state.ui_tuner_icon, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

	g_source_state.radio_freq_label = lv_label_create(g_source_state.radio_panel);
    lv_obj_set_width(g_source_state.radio_freq_label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.radio_freq_label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.radio_freq_label, 200);
    lv_obj_set_y(g_source_state.radio_freq_label, 100);
	 __msg("g_radio_data_cache.band=%d\n",g_radio_data_cache.band);
	  __msg("g_radio_data_cache.frequency=%d\n",g_radio_data_cache.frequency);
	 if(g_radio_data_cache.band==0)
        snprintf(freq_buf, sizeof(freq_buf), "%.1f MHz", (float)g_radio_data_cache.frequency / 1000.0f);
    else
        snprintf(freq_buf, sizeof(freq_buf), "%d KHz", g_radio_data_cache.frequency);
    
    lv_label_set_text(g_source_state.radio_freq_label, freq_buf);
    //lv_label_set_text(g_source_state.radio_freq_label, "87.5MHz");
    lv_obj_set_style_text_color(g_source_state.radio_freq_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.radio_freq_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.radio_freq_label, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

		g_source_state.radio_pty_name_label = lv_label_create(g_source_state.radio_panel);
    lv_obj_set_width(g_source_state.radio_pty_name_label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.radio_pty_name_label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.radio_pty_name_label, 200);
    lv_obj_set_y(g_source_state.radio_pty_name_label, 140);
	eLIBs_strcpy(pty_data,fm_res_rds_pty_string[g_radio_data_cache.pty]);
    lv_label_set_text(g_source_state.radio_pty_name_label, pty_data);
    
    lv_obj_set_style_text_color(g_source_state.radio_pty_name_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.radio_pty_name_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.radio_pty_name_label, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

		g_source_state.radio_ps_name_label = lv_label_create(g_source_state.radio_panel);
    lv_obj_set_width(g_source_state.radio_ps_name_label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.radio_ps_name_label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.radio_ps_name_label, 200);
    lv_obj_set_y(g_source_state.radio_ps_name_label, 180);
	lv_label_set_text(g_source_state.radio_ps_name_label, g_radio_data_cache.ps_name);
    lv_obj_set_style_text_color(g_source_state.radio_ps_name_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.radio_ps_name_label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.radio_ps_name_label, lv_font_medium.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_source_setup = lv_imgbtn_create(g_source_state.radio_panel);
    lv_imgbtn_set_src(g_source_state.ui_source_setup, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_SET_UP_N_BMP), NULL);	
    lv_imgbtn_set_src(g_source_state.ui_source_setup, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_SET_UP_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_source_setup, 36);
    lv_obj_set_height(g_source_state.ui_source_setup, 16);
    lv_obj_set_x(g_source_state.ui_source_setup, 337);
    lv_obj_set_y(g_source_state.ui_source_setup, 38);

    g_source_state.ui_tuner_set_pancel = lv_obj_create(g_source_state.radio_panel);
    lv_obj_set_height(g_source_state.ui_tuner_set_pancel, 50);
    lv_obj_set_width(g_source_state.ui_tuner_set_pancel, lv_pct(100));
    lv_obj_set_x(g_source_state.ui_tuner_set_pancel, 0);
    lv_obj_set_y(g_source_state.ui_tuner_set_pancel, 280);
    lv_obj_set_flex_flow(g_source_state.ui_tuner_set_pancel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_source_state.ui_tuner_set_pancel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_tuner_set_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_tuner_set_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(g_source_state.ui_tuner_set_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_add_preset_button = lv_imgbtn_create(g_source_state.ui_tuner_set_pancel);
    lv_imgbtn_set_src(g_source_state.ui_add_preset_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_ADD_PRESET_N_BMP), NULL);
    lv_imgbtn_set_src(g_source_state.ui_add_preset_button, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_ADD_PRESET_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_add_preset_button, 49);
    lv_obj_set_height(g_source_state.ui_add_preset_button, 47);
    lv_obj_set_x(g_source_state.ui_add_preset_button, 7);
    lv_obj_set_y(g_source_state.ui_add_preset_button, -10);
	lv_obj_add_flag(g_source_state.ui_add_preset_button, LV_OBJ_FLAG_CHECKABLE);
	if(g_radio_data_cache.preset_index!=0)
		lv_imgbtn_set_state(g_source_state.ui_add_preset_button, LV_IMGBTN_STATE_CHECKED_RELEASED);

	
	g_source_state.ui_add_preset_num = lv_label_create(g_source_state.ui_add_preset_button);
	lv_obj_set_width(g_source_state.ui_add_preset_num, LV_SIZE_CONTENT);	/// 1
	lv_obj_set_height(g_source_state.ui_add_preset_num, LV_SIZE_CONTENT);	  /// 1
	
	lv_obj_set_x(g_source_state.ui_add_preset_num, 29);
	lv_obj_set_y(g_source_state.ui_add_preset_num, 18);
	sprintf(preset,"%d",g_radio_data_cache.preset_index);
	lv_label_set_text(g_source_state.ui_add_preset_num, preset);
	lv_obj_set_style_text_color(g_source_state.ui_add_preset_num, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_opa(g_source_state.ui_add_preset_num, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(g_source_state.ui_add_preset_num, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);
	if(g_radio_data_cache.preset_index==0)
		lv_obj_add_flag(g_source_state.ui_add_preset_num, LV_OBJ_FLAG_HIDDEN);

    g_source_state.ui_tuner_eq_button = lv_imgbtn_create(g_source_state.ui_tuner_set_pancel);
    lv_imgbtn_set_src(g_source_state.ui_tuner_eq_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_EQ_N_BMP), NULL);
    lv_imgbtn_set_src(g_source_state.ui_tuner_eq_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_EQ_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_tuner_eq_button, 53);
    lv_obj_set_height(g_source_state.ui_tuner_eq_button, 51);
    lv_obj_set_x(g_source_state.ui_tuner_eq_button, 85);
    lv_obj_set_y(g_source_state.ui_tuner_eq_button, -10);

    g_source_state.ui_auto_store_button = lv_imgbtn_create(g_source_state.ui_tuner_set_pancel);
	lv_imgbtn_set_src(g_source_state.ui_auto_store_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_AUTO_STORE_N_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_auto_store_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_AUTO_STORE_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_auto_store_button, 50);
    lv_obj_set_height(g_source_state.ui_auto_store_button, 50);
    lv_obj_set_x(g_source_state.ui_auto_store_button, 180);
    lv_obj_set_y(g_source_state.ui_auto_store_button, -9);

    g_source_state.ui_tuner_list_button = lv_imgbtn_create(g_source_state.ui_tuner_set_pancel);
	lv_imgbtn_set_src(g_source_state.ui_tuner_list_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_PRESET_LIST_N_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_tuner_list_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_PRESET_LIST_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_tuner_list_button, 56);
    lv_obj_set_height(g_source_state.ui_tuner_list_button, 47);
    lv_obj_set_x(g_source_state.ui_tuner_list_button, 275);
    lv_obj_set_y(g_source_state.ui_tuner_list_button, -10);

    g_source_state.ui_freq_panel = lv_obj_create(g_source_state.radio_panel);
    lv_obj_set_height(g_source_state.ui_freq_panel, 68);
    lv_obj_set_width(g_source_state.ui_freq_panel, lv_pct(100));
    lv_obj_set_x(g_source_state.ui_freq_panel, 0);
    lv_obj_set_y(g_source_state.ui_freq_panel, 352);
    lv_obj_set_scrollbar_mode(g_source_state.ui_freq_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_freq_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.ui_freq_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	__msg("g_radio_data_cache.max_freq=%d\n",g_radio_data_cache.max_freq);
	__msg("g_radio_data_cache.tu_step=%d\n",g_radio_data_cache.tu_step);
	__msg("g_radio_data_cache.min_freq=%d\n",g_radio_data_cache.min_freq);
	g_radio_data_cache.min_freq = 87500;
	g_radio_data_cache.tu_step  = 100;
	g_radio_data_cache.max_freq = 108000;

    lv_img_dsc_t * scale_img_dsc = ui_source_get_res(&g_source_state,TUNER_FREQ_SCALE_BMP);
    g_source_state.freq_scale_img_w = scale_img_dsc->header.w;
    g_source_state.freq_scale_setp_w = g_source_state.freq_scale_img_w / 6;
    
    __u8 freq_num = (g_radio_data_cache.max_freq - g_radio_data_cache.min_freq) / (g_radio_data_cache.tu_step * 5);
    __u32 cur_frq;
    __u32 L_freq_array[] = {106500, 107000, 107500, 108000};
    __u32 R_freq_array[] = {87500, 88000, 88500, 89000};
    
	__msg("freq_num=%d\n",freq_num);
	__msg("freq_scale_img_w=%d, freq_scale_setp_w=%d\n", g_source_state.freq_scale_img_w, g_source_state.freq_scale_setp_w);
    
    g_source_state.ui_freq_scale = lv_img_create(g_source_state.ui_freq_panel);
    lv_img_set_src(g_source_state.ui_freq_scale, ui_source_get_res(&g_source_state,TUNER_FREQ_SCALE_BMP));
    lv_obj_set_width(g_source_state.ui_freq_scale, g_source_state.freq_scale_img_w * 7  + g_source_state.freq_scale_setp_w * 7);
    lv_obj_set_height(g_source_state.ui_freq_scale, LV_SIZE_CONTENT);    /// 1
    lv_obj_clear_flag(g_source_state.ui_freq_scale, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(g_source_state.ui_freq_scale, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    g_source_state.ui_freq_text_panel = lv_obj_create(g_source_state.ui_freq_panel);
    lv_obj_set_height(g_source_state.ui_freq_text_panel, LV_SIZE_CONTENT);
    lv_obj_set_width(g_source_state.ui_freq_text_panel, g_source_state.freq_scale_img_w * 7  + g_source_state.freq_scale_setp_w * 7 + g_source_state.freq_scale_setp_w);
    lv_obj_set_x(g_source_state.ui_freq_text_panel, -30);//(g_source_state.freq_scale_setp_w / 2)
    lv_obj_set_align(g_source_state.ui_freq_text_panel, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_flex_flow(g_source_state.ui_freq_text_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_source_state.ui_freq_text_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_freq_text_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_freq_text_panel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.ui_freq_text_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	for(i = 0; i < 4; i++)
	{
	    snprintf(freq_buf, sizeof(freq_buf), "%.1f", (float)L_freq_array[i] / 1000.0f);
        __msg("freq_buf=%s\n",freq_buf);

        lv_obj_t * ui_freq_text = lv_label_create(g_source_state.ui_freq_text_panel);
        lv_obj_set_height(ui_freq_text, LV_SIZE_CONTENT);
        lv_obj_set_width(ui_freq_text, g_source_state.freq_scale_setp_w);   /// 1
        lv_label_set_text(ui_freq_text, freq_buf);
        lv_obj_set_style_text_color(ui_freq_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_freq_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_freq_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_freq_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	for(i = 0; i < freq_num + 1; i++)
	{
        cur_frq = g_radio_data_cache.min_freq + (i * g_radio_data_cache.tu_step * 5);
        __msg("cur_frq=%d\n",cur_frq);
	    snprintf(freq_buf, sizeof(freq_buf), "%.1f", (float)cur_frq / 1000.0f);
        __msg("freq_buf=%s\n",freq_buf);

        lv_obj_t * ui_freq_text = lv_label_create(g_source_state.ui_freq_text_panel);
        lv_obj_set_height(ui_freq_text, LV_SIZE_CONTENT);
        lv_obj_set_width(ui_freq_text, g_source_state.freq_scale_setp_w);   /// 1
        lv_label_set_text(ui_freq_text, freq_buf);
        lv_obj_set_style_text_color(ui_freq_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_freq_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_freq_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_freq_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	for(i = 0; i < 4; i++)
	{
	    snprintf(freq_buf, sizeof(freq_buf), "%.1f", (float)R_freq_array[i] / 1000.0f);
        __msg("freq_buf=%s\n",freq_buf);

        lv_obj_t * ui_freq_text = lv_label_create(g_source_state.ui_freq_text_panel);
        lv_obj_set_height(ui_freq_text, LV_SIZE_CONTENT);
        lv_obj_set_width(ui_freq_text, g_source_state.freq_scale_setp_w);   /// 1
        lv_label_set_text(ui_freq_text, freq_buf);
        lv_obj_set_style_text_color(ui_freq_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_freq_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_freq_text, lv_font_ssmall.font, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_align(ui_freq_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

    g_source_state.ui_freq_red_scale_line = lv_obj_create(g_source_state.radio_panel);
    lv_obj_set_height(g_source_state.ui_freq_red_scale_line, 45);
    lv_obj_set_width(g_source_state.ui_freq_red_scale_line, 3);
    lv_obj_set_x(g_source_state.ui_freq_red_scale_line, 192);
    lv_obj_set_y(g_source_state.ui_freq_red_scale_line, 352);
    // lv_obj_set_flex_flow(g_source_state.ui_freq_text_panel, LV_FLEX_FLOW_ROW);
    // lv_obj_set_flex_align(g_source_state.ui_freq_red_scale_line, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    //lv_obj_set_align(g_source_state.ui_freq_red_scale_line, LV_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_freq_red_scale_line, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_add_flag(g_source_state.ui_freq_red_scale_line, LV_OBJ_FLAG_IGNORE_LAYOUT);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_freq_red_scale_line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_freq_red_scale_line, lv_color_hex(GUI_RED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_freq_red_scale_line, 0xffffff, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_freq_red_scale_line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_update_layout(g_source_state.ui_freq_panel);
    g_source_state.freq_panel_w = lv_obj_get_width(g_source_state.ui_freq_panel);
    g_source_state.freq_scale_w = lv_obj_get_width(g_source_state.ui_freq_scale);
    __msg("freq_panel_w=%d, freq_scale_w=%d\n", g_source_state.freq_panel_w, g_source_state.freq_scale_w);

    g_source_state.ui_tuner_func_pancel = lv_obj_create(g_source_state.radio_panel);
    lv_obj_set_height(g_source_state.ui_tuner_func_pancel, 70);
    lv_obj_set_width(g_source_state.ui_tuner_func_pancel, lv_pct(100));
    lv_obj_set_x(g_source_state.ui_tuner_func_pancel, 0);
    lv_obj_set_y(g_source_state.ui_tuner_func_pancel, 454);
    lv_obj_set_flex_flow(g_source_state.ui_tuner_func_pancel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_source_state.ui_tuner_func_pancel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_tuner_func_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_tuner_func_pancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.ui_tuner_func_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_prev_button = lv_imgbtn_create(g_source_state.ui_tuner_func_pancel);
	lv_imgbtn_set_src(g_source_state.ui_prev_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_PREV_N_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_prev_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_PREV_P_BMP), NULL);
    lv_obj_set_width(g_source_state.ui_prev_button, 52);
    lv_obj_set_height(g_source_state.ui_prev_button, 59);
    lv_obj_set_x(g_source_state.ui_prev_button, 52);
    lv_obj_set_y(g_source_state.ui_prev_button, 5);

    g_source_state.ui_band_button = lv_switch_create(g_source_state.ui_tuner_func_pancel);
    lv_obj_set_width(g_source_state.ui_band_button, 140);
    lv_obj_set_height(g_source_state.ui_band_button, 70);
    lv_obj_set_x(g_source_state.ui_band_button, 125);
    lv_obj_set_y(g_source_state.ui_band_button, 0);


	lv_obj_set_style_radius(g_source_state.ui_band_button, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.ui_band_button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.ui_band_button, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_grad_color(g_source_state.ui_band_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	
	lv_obj_set_style_radius(g_source_state.ui_band_button, 20, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(g_source_state.ui_band_button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(g_source_state.ui_band_button, 128, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_grad_color(g_source_state.ui_band_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_CHECKED);

	lv_obj_set_style_radius(g_source_state.ui_band_button, 20, LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(g_source_state.ui_band_button, lv_color_hex(0x00000), LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(g_source_state.ui_band_button, 128, LV_PART_INDICATOR | LV_STATE_CHECKED);

	lv_obj_set_style_radius(g_source_state.ui_band_button, 20, LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.ui_band_button, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.ui_band_button, 255, LV_PART_KNOB | LV_STATE_DEFAULT);

	if(g_radio_data_cache.band==0)
		lv_obj_add_state(g_source_state.ui_band_button, LV_STATE_DEFAULT);
	else
		lv_obj_add_state(g_source_state.ui_band_button, LV_STATE_CHECKED);
	
    g_source_state.ui_fm_text = lv_label_create(g_source_state.ui_band_button);
    lv_obj_set_width(g_source_state.ui_fm_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_fm_text, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_fm_text, 13);
    lv_obj_set_y(g_source_state.ui_fm_text, 22);
    lv_label_set_text(g_source_state.ui_fm_text, "FM");
    lv_obj_set_style_text_align(g_source_state.ui_fm_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.ui_fm_text, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_am_text = lv_label_create(g_source_state.ui_band_button);
    lv_obj_set_width(g_source_state.ui_am_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_am_text, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_am_text, 83);
    lv_obj_set_y(g_source_state.ui_am_text, 24);
    lv_label_set_text(g_source_state.ui_am_text, "AM");
    lv_obj_set_style_text_color(g_source_state.ui_am_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.ui_am_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(g_source_state.ui_am_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.ui_am_text, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_next_button = lv_imgbtn_create(g_source_state.ui_tuner_func_pancel);
	lv_imgbtn_set_src(g_source_state.ui_next_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,TUNER_NEXT_N_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_next_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,TUNER_NEXT_P_BMP), NULL);
    lv_obj_set_height(g_source_state.ui_next_button, 64);
    lv_obj_set_width(g_source_state.ui_next_button, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(g_source_state.ui_next_button, 294);
    lv_obj_set_y(g_source_state.ui_next_button, 5);

    g_source_state.ui_source_sel_pancel = lv_obj_create( g_source_state.radio_play_panel);
    lv_obj_set_height(g_source_state.ui_source_sel_pancel, 120);
    lv_obj_set_width(g_source_state.ui_source_sel_pancel, lv_pct(100));
    lv_obj_set_x(g_source_state.ui_source_sel_pancel, 0);
    lv_obj_set_y(g_source_state.ui_source_sel_pancel, 556);
    lv_obj_set_flex_flow(g_source_state.ui_source_sel_pancel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_source_state.ui_source_sel_pancel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_source_state.ui_source_sel_pancel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(g_source_state.ui_source_sel_pancel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_source_state.ui_source_sel_pancel, lv_color_hex(0x2B2929), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(g_source_state.ui_source_sel_pancel, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_source_state.ui_source_sel_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(g_source_state.ui_source_sel_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(g_source_state.ui_source_sel_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(g_source_state.ui_source_sel_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(g_source_state.ui_source_sel_pancel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_source_tuner_button = lv_imgbtn_create(g_source_state.ui_source_sel_pancel);
	lv_imgbtn_set_src(g_source_state.ui_source_tuner_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,SOURCE_TUNE_BUTTON_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_source_tuner_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,SOURCE_TUNE_BUTTON_BMP), NULL);
    lv_obj_set_height(g_source_state.ui_source_tuner_button, 70);
    lv_obj_set_width(g_source_state.ui_source_tuner_button, LV_SIZE_CONTENT);   /// 1

    g_source_state.ui_source_usb_button = lv_imgbtn_create(g_source_state.ui_source_sel_pancel);
	lv_imgbtn_set_src(g_source_state.ui_source_usb_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,SOURCE_USB_BUTTON_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_source_usb_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,SOURCE_USB_BUTTON_BMP), NULL);
    lv_obj_set_height(g_source_state.ui_source_usb_button, 70);
    lv_obj_set_width(g_source_state.ui_source_usb_button, LV_SIZE_CONTENT);   /// 1

    g_source_state.ui_source_bt_button = lv_imgbtn_create(g_source_state.ui_source_sel_pancel);
	lv_imgbtn_set_src(g_source_state.ui_source_bt_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,SOURCE_BT_BUTTON_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_source_bt_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,SOURCE_BT_BUTTON_BMP), NULL);
    lv_obj_set_height(g_source_state.ui_source_bt_button, 70);
    lv_obj_set_width(g_source_state.ui_source_bt_button, LV_SIZE_CONTENT);   /// 1

    g_source_state.ui_source_more_button = lv_imgbtn_create(g_source_state.ui_source_sel_pancel);
	lv_imgbtn_set_src(g_source_state.ui_source_more_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,SOURCE_MORE_BUTTON_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.ui_source_more_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,SOURCE_MORE_BUTTON_BMP), NULL);
    lv_obj_set_height(g_source_state.ui_source_more_button, 70);
    lv_obj_set_width(g_source_state.ui_source_more_button, LV_SIZE_CONTENT);   /// 1

	ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);
	//source_Draw_album_tags_pg(&g_source_state,g_source_state.ui_source_bg_panel,g_source_state.lv_source_icon[TUNER_FM_ICON_BG_BMP].data);


	g_source_state.radio_list_panel = lv_obj_create(parent);
	lv_obj_set_height(g_source_state.radio_list_panel, lv_pct(100));
	lv_obj_set_width(g_source_state.radio_list_panel, lv_pct(100));
	lv_obj_set_x(g_source_state.radio_list_panel, 0);
	lv_obj_set_y(g_source_state.radio_list_panel, 0);
	lv_obj_clear_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	lv_obj_add_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_HIDDEN); 	/// Flags
	lv_obj_set_style_radius(g_source_state.radio_list_panel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.radio_list_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_source_state.radio_list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	
	//lv_obj_add_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN); 	/// Flags
	

	  g_source_state.radio_list_back_button = lv_imgbtn_create(g_source_state.radio_list_panel);
	lv_imgbtn_set_src(g_source_state.radio_list_back_button, LV_IMGBTN_STATE_RELEASED, NULL, ui_source_get_res(&g_source_state,SOURCE_LIST_RETURN_N_BMP), NULL);
	 lv_imgbtn_set_src(g_source_state.radio_list_back_button, LV_IMGBTN_STATE_PRESSED, NULL, ui_source_get_res(&g_source_state,SOURCE_LIST_RETURN_N_BMP), NULL);
    lv_obj_set_height(g_source_state.radio_list_back_button, 26);
    lv_obj_set_width(g_source_state.radio_list_back_button, 26);   /// 1
    lv_obj_set_x(g_source_state.radio_list_back_button, 25);
    lv_obj_set_y(g_source_state.radio_list_back_button, 36);

		g_source_state.radio_list_band_button = lv_switch_create(g_source_state.radio_list_panel);
    lv_obj_set_width(g_source_state.radio_list_band_button, 140);
    lv_obj_set_height(g_source_state.radio_list_band_button, 60);
    lv_obj_set_x(g_source_state.radio_list_band_button, 125);
    lv_obj_set_y(g_source_state.radio_list_band_button, 15);


	lv_obj_set_style_radius(g_source_state.radio_list_band_button, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.radio_list_band_button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.radio_list_band_button, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_grad_color(g_source_state.radio_list_band_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	
	lv_obj_set_style_radius(g_source_state.radio_list_band_button, 20, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(g_source_state.radio_list_band_button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(g_source_state.radio_list_band_button, 128, LV_PART_MAIN | LV_STATE_CHECKED);
	lv_obj_set_style_bg_grad_color(g_source_state.radio_list_band_button, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_CHECKED);

	lv_obj_set_style_radius(g_source_state.radio_list_band_button, 20, LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_color(g_source_state.radio_list_band_button, lv_color_hex(0x00000), LV_PART_INDICATOR | LV_STATE_CHECKED);
	lv_obj_set_style_bg_opa(g_source_state.radio_list_band_button, 128, LV_PART_INDICATOR | LV_STATE_CHECKED);

	lv_obj_set_style_radius(g_source_state.radio_list_band_button, 20, LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.radio_list_band_button, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.radio_list_band_button, 255, LV_PART_KNOB | LV_STATE_DEFAULT);

	if(g_radio_data_cache.band==0)
		lv_obj_add_state(g_source_state.radio_list_band_button, LV_STATE_DEFAULT);
	else
		lv_obj_add_state(g_source_state.radio_list_band_button, LV_STATE_CHECKED);
	
    g_source_state.ui_list_fm_text = lv_label_create(g_source_state.radio_list_band_button);
    lv_obj_set_width(g_source_state.ui_list_fm_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_list_fm_text, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_list_fm_text, 13);
    lv_obj_set_y(g_source_state.ui_list_fm_text, 14);
    lv_label_set_text(g_source_state.ui_list_fm_text, "FM");
    lv_obj_set_style_text_align(g_source_state.ui_list_fm_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.ui_list_fm_text, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_source_state.ui_list_am_text = lv_label_create(g_source_state.radio_list_band_button);
    lv_obj_set_width(g_source_state.ui_list_am_text, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(g_source_state.ui_list_am_text, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(g_source_state.ui_list_am_text, 87);
    lv_obj_set_y(g_source_state.ui_list_am_text, 14);
    lv_label_set_text(g_source_state.ui_list_am_text, "AM");
    lv_obj_set_style_text_color(g_source_state.ui_list_am_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(g_source_state.ui_list_am_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(g_source_state.ui_list_am_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_source_state.ui_list_am_text, lv_font_small.font, LV_PART_MAIN | LV_STATE_DEFAULT);

	
	g_source_state.radio_list = lv_obj_create(g_source_state.radio_list_panel);
	lv_obj_set_height(g_source_state.radio_list, 540);
	lv_obj_set_width(g_source_state.radio_list, lv_pct(100));
	lv_obj_set_x(g_source_state.radio_list, 0);
	lv_obj_set_y(g_source_state.radio_list, 80);
	lv_obj_clear_flag(g_source_state.radio_list, LV_OBJ_FLAG_SCROLLABLE); 	 /// Flags
	//lv_obj_clear_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_HIDDEN);	   /// Flags
	lv_obj_set_style_radius(g_source_state.radio_list, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(g_source_state.radio_list, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_width(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(g_source_state.radio_list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_obj_clear_flag(g_source_state.radio_play_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	lv_obj_add_flag(g_source_state.radio_list_panel, LV_OBJ_FLAG_HIDDEN);      /// Flags
	if(g_source_state.cur_tuner_win == TUNER_PLAY_WIN)
	{
		lv_obj_add_event_cb(g_source_state.ui_source_setup, ui_event_fm_set_up, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_add_preset_button, ui_event_fm_add_preset, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_tuner_eq_button, ui_event_fm_eq, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_auto_store_button, ui_event_fm_auto_store, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_tuner_list_button, ui_event_fm_list, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_prev_button, ui_event_fm_prev, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_band_button, ui_event_fm_switch_band,LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_next_button, ui_event_fm_next, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.ui_freq_panel, ui_event_fm_freq_panel, LV_EVENT_ALL, &g_source_state);
	}
	else if(g_source_state.cur_tuner_win == TUNER_LIST_WIN)
	{
		lv_obj_add_event_cb(g_source_state.radio_list_back_button, ui_event_fm_list_return, LV_EVENT_ALL, &g_source_state);
		lv_obj_add_event_cb(g_source_state.radio_list_band_button, ui_event_fm_list_switch_band,LV_EVENT_ALL, &g_source_state);
	}
	
    
	//lv_timer_create(ui_event_source_timer_cb, 1, &g_source_state);
   
}

/**
 * @brief 创建 DAB 播放器界面 (占位)
 */
static void create_dab_panel(lv_obj_t* parent) {
    g_source_state.dab_panel = lv_label_create(parent);
    lv_label_set_text(g_source_state.dab_panel, "DAB Player UI");
    lv_obj_center(g_source_state.dab_panel);
}

/**
 * @brief 创建蓝牙音乐播放器界面 (占位)
 */
static void create_bt_panel(lv_obj_t* parent) {
    g_source_state.bt_panel = lv_label_create(parent);
    lv_label_set_text(g_source_state.bt_panel, "Bluetooth Music Player UI");
    lv_obj_center(g_source_state.bt_panel);
}

/**
 * @brief 创建 USB 音乐播放器界面 (占位)
 */
static void create_usb_panel(lv_obj_t* parent) {
    g_source_state.usb_panel = lv_label_create(parent);
    lv_label_set_text(g_source_state.usb_panel, "USB Music Player UI");
    lv_obj_center(g_source_state.usb_panel);
}

/**
 * @brief 切换音源面板（FM/AM、DAB、BT、USB）
 * @param new_source 要切换到的音源类型
 */
static void switch_source_panel(ui_source_type_t new_source) {
    // 1. 隐藏所有面板
    if (g_source_state.radio_panel) lv_obj_add_flag(g_source_state.radio_panel, LV_OBJ_FLAG_HIDDEN);
    if (g_source_state.dab_panel) lv_obj_add_flag(g_source_state.dab_panel, LV_OBJ_FLAG_HIDDEN);
    if (g_source_state.bt_panel) lv_obj_add_flag(g_source_state.bt_panel, LV_OBJ_FLAG_HIDDEN);
    if (g_source_state.usb_panel) lv_obj_add_flag(g_source_state.usb_panel, LV_OBJ_FLAG_HIDDEN);

    // 2. 显示目标面板，并预留动画接口
    lv_obj_t* show_panel = NULL;
    switch (new_source) {
        case UI_SOURCE_TYPE_FM:
        case UI_SOURCE_TYPE_AM:
            show_panel = g_source_state.radio_panel;
            break;
        case UI_SOURCE_TYPE_DAB:
            show_panel = g_source_state.dab_panel;
            break;
        case UI_SOURCE_TYPE_BT_MUSIC:
            show_panel = g_source_state.bt_panel;
            break;
        case UI_SOURCE_TYPE_USB_MUSIC:
            show_panel = g_source_state.usb_panel;
            break;
        default:
            break;
    }
    if (show_panel) {
        lv_obj_clear_flag(show_panel, LV_OBJ_FLAG_HIDDEN);
        // 预留：切换动画接口
        // lv_anim_t a;
        // lv_anim_init(&a);
        // lv_anim_set_var(&a, show_panel);
        // lv_anim_set_values(&a, 0, 255); // 例如淡入
        // lv_anim_set_time(&a, 300);
        // lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        // lv_anim_start(&a);
    }
    // 3. 更新当前音源类型
    create_cfg.initial_source = new_source;
}





/**
 * @brief 核心：接收并处理来自后端服务的消息
 */
static int source_widget_handle_event(ui_manger_t* manger, const app_message_t* msg) {

    // A. 处理来自 System Service 的控制指令
    if (msg->source == SRC_SYSTEM_SERVICE) {
        if (msg->command == SYSTEM_EVT_READY) {
            if (!msg->payload) return 0;
            ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
            system_state_payload_t* payload = (system_state_payload_t*)wrapper->ptr;
            if (payload) {
                // 更新本控件关心的初始音源类型
                if(manger->is_created && payload->initial_source_type!= create_cfg.initial_source)
                {
                    switch_source_panel(payload->initial_source_type);
                }
                create_cfg.initial_source = payload->initial_source_type;
                printf("UI Source: Initial source type set to %d by System Service.\n", create_cfg.initial_source);
            }
        }
        return 0; // System Service 的消息处理完毕
    }

    // B. 处理来自 Radio Service 的数据更新
    if (msg->source == SRC_RADIO_SERVICE) {
#if 1
	switch(msg->command)
	{
		case RADIO_EVT_READY:
		{
			  __msg("RADIO_EVT_READY\n");
			  if (!msg->payload) return 0;
			   ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
		            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;

		            // 1. 无论如何，都先更新数据缓存
		            //g_radio_data_cache = *radio_data;
		            memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
		            printf("UI Source: Radio data cache updated. Freq: %d\n", g_radio_data_cache.frequency);

		            // 2. 判断当前音源是否是被激活的(Radio音源)
		            bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
				g_source_state.cur_band = g_radio_data_cache.band;
		            // 3. 只有在UI已创建且当前是Radio音源时，才刷新UI
		            if (manger->is_created && is_radio_source_active) {
		                printf("UI Source: Radio is active, refreshing UI.\n");
		                refresh_radio_ui_from_cache();
		            } else {
		                printf("UI Source: Radio is not active or UI not created, only cache updated.\n");
		            }
		}
		break;
		case RADIO_EVT_UPDATE_FREQUENCY:
	 	{
			 
			 static __u8 cur_band=0xff;
			 __s32 freq_scale_start_x=0;
			  //__msg("RADIO_EVT_UPDATE_FREQUENCY\n");
	            if (!msg->payload) return 0;
			   ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
		            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;
				 //g_radio_data_cache = *radio_data;	
				  memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
				   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
				 if (!manger->is_created||!is_radio_source_active)
				 	return;
				 if(g_source_state.cur_band!=g_radio_data_cache.band)
				{
					g_source_state.cur_band = g_radio_data_cache.band;
					ui_tuner_update_band();
				}
				else
				{
					ui_tuner_update_freq_scale();
					ui_tuner_update_freq();
					ui_tuner_update_perset();
					__msg("g_radio_data_cache.frequency=%d\n",g_radio_data_cache.frequency);
					
				}
				   //lv_label_set_text(ui->band_label, payload->band_str);
            }
            break;

        case RADIO_EVT_UPDATE_BAND:
            {
				
		  if (!msg->payload) return 0;
			   ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
		            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;	
				 //g_radio_data_cache = *radio_data;	
				  memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
				    bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
				 if (!manger->is_created||!is_radio_source_active)
				 	return;
			 //strcpy(g_radio_data_cache.band_str,radio_data->band_str);	
			 //__msg("g_radio_data_cache.band_str=%s\n",g_radio_data_cache.band_str);
			 if(g_radio_data_cache.band==0)
			 {
				 lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_AM_ICON_BG_BMP));
				//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
				ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);
			 }
			 else
		 	{
		 		lv_img_set_src(g_source_state.ui_tuner_big_icon_bg, ui_source_get_res(&g_source_state,TUNER_FM_ICON_BG_BMP));
				//ui_canvas_blur_uninit(ui_source_canvas_snapshot);
				ui_canvas_blur_init(g_source_state.ui_source_bg_panel,g_source_state.ui_tuner_big_icon_bg,40);
		 	}
                //lv_label_set_text(g_source_state.ui_fm_text, g_radio_data_cache.band_str);
            }
            break;

        case RADIO_EVT_UPDATE_RDS_INFO:
             {
		if (!msg->payload) return 0;
		char pty_data[32]={0};
		   ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
	            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;		
			//g_radio_data_cache = *radio_data;	
			 memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
			   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
			 if (!manger->is_created||!is_radio_source_active)
			 	return;
			ui_tuner_update_ps_pty(1);
            }
            break;
	case RADIO_EVT_SEARCH_STARTED:
             {
		if (!msg->payload) return 0;
		char pty_data[32]={0};
		   ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
	            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;		
			//g_radio_data_cache = *radio_data;	
			__msg("g_radio_data_cache.frequency=%d\n",g_radio_data_cache.frequency);
			 memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
			   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
			 if (!manger->is_created||!is_radio_source_active)
			 	return;
			//strcpy(g_radio_data_cache.pty_name,radio_data->pty_name);
			
			ui_tuner_update_seek(g_radio_data_cache.cur_seeking_type);
            }
            break;		
	case RADIO_EVT_SEARCH_FINISHED:
		if (!msg->payload) return 0;
		   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
				 if (!manger->is_created||!is_radio_source_active)
				 	return;
             ui_tuner_update_ps_pty(0);
            break;	
	case RADIO_EVT_SWITCH_BAND_STARTED:
	{
		if (!msg->payload) return 0;
		   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
			 if (!manger->is_created||!is_radio_source_active)
			 	return;
		ui_tuner_clear_freq_scale();
	}
            break;	
	case RADIO_EVT_SWITCH_BAND_FINISHED:
		if (!msg->payload) return 0;
            
            break;	
	case RADIO_EVT_UPDATE_PRESET_LIST:
		{
			if (!msg->payload) 
				return 0;
	   		ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
	            	radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;		
			//g_radio_data_cache = *radio_data;
			 memcpy(&g_radio_data_cache, radio_data, sizeof(radio_state_payload_t));
			   bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
		                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);
			 if (!manger->is_created||!is_radio_source_active)
			 	return;
			__msg("RADIO_EVT_UPDATE_PRESET_LIST\n");
			__tuner_list_update_preset(&g_source_state,1);
		}
		break;
	}
	 
#else
        if (msg->command == RADIO_EVT_READY) { // 假设这是统一的状态更新事件
            if (!msg->payload) return 0;
            ref_counted_payload_t* wrapper = (ref_counted_payload_t*)msg->payload;
            radio_state_payload_t* radio_data = (radio_state_payload_t*)wrapper->ptr;

            // 1. 无论如何，都先更新数据缓存
            g_radio_data_cache = *radio_data;
            printf("UI Source: Radio data cache updated. Freq: %d\n", g_radio_data_cache.frequency);

            // 2. 判断当前音源是否是被激活的(Radio音源)
            bool is_radio_source_active = (create_cfg.initial_source == UI_SOURCE_TYPE_FM ||
                                           create_cfg.initial_source == UI_SOURCE_TYPE_AM);

            // 3. 只有在UI已创建且当前是Radio音源时，才刷新UI
            if (manger->is_created && is_radio_source_active) {
                printf("UI Source: Radio is active, refreshing UI.\n");
                refresh_radio_ui_from_cache();
            } else {
                printf("UI Source: Radio is not active or UI not created, only cache updated.\n");
            }

        }
#endif		
    }
    
    // C. 在此添加对其他服务(BT, USB)消息的处理
    // else if (msg->source == SRC_BT_SERVICE) { ... }
    return 0;
}


