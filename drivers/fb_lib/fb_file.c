/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "apps.h"
#include "fb_file.h"
#include <string.h>
#include <log.h>
#include <libc/eLIBs_az100.h>
#include "elibs_libjpegdec.h"
#include <emodules/mod_display.h>
#include <kconfig.h>
//#include "init/init.h"
#include "bg_bmp.h"

#if 0
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_PREFIX, SYS_LOG_COLOR_PURPLE, ##__VA_ARGS__); } while(0)
#define __inf(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_INFO, LOG_LEVEL_INFO_PREFIX, SYS_LOG_COLOR_BLUE, ##__VA_ARGS__); } while(0)
#define __msg(...)    do { LOG_NO_COLOR(OPTION_LOG_LEVEL_MESSAGE, LOG_LEVEL_VERBOSE_PREFIX, ##__VA_ARGS__); } while(0)
#endif

#if 0//def SKILL_FOR_SHUN_LISTCHANNEL_REPEATED //mllee 110824 
	#define __testmsg(...) (eLIBs_printf("WRN:L%d(%s):", __LINE__, __FILE__),	eLIBs_printf(__VA_ARGS__))
#else
	#define __testmsg(...) 
	
#endif 

/************************************************************************
* Function: backlayer_create_fb_file
* Description: 保存fb到文件
* Input:
*    FB *fb：待保存的fb
*    uint8_t * filename：保存fb的文件名
* Output:
* Return:
*     EPDK_OK: 成功
*     EPDK_FAIL: 失败
*************************************************************************/
int32_t create_fb_file(FB *framebuffer, uint8_t *filename)
{
    __hdle fd;
    __fb_file_header_t file_header;
    int32_t write_num;

    fd = fopen((char *)filename, "w+");

    if (fd == NULL)
    {
        __wrn("------open file:%s err!------", filename);
        return EPDK_FAIL;
    }

    memset(&file_header, 0, sizeof(file_header));
    file_header.version = 0x100;
    file_header.height = framebuffer->size.height;
    file_header.width = framebuffer->size.width;
    file_header.cs_mode = framebuffer->fmt.cs_mode;
    file_header.fmt_type = framebuffer->fmt.type;

    if (framebuffer->fmt.type == FB_TYPE_RGB)
    {
        file_header.pix_fmt = framebuffer->fmt.fmt.rgb.pixelfmt;
        file_header.pix_seq = framebuffer->fmt.fmt.rgb.pixseq;
        file_header.mod_or_br_swap = framebuffer->fmt.fmt.rgb.br_swap;
        file_header.data_len[0] = file_header.height * file_header.width * 4;
    }
    else
    {
        if (framebuffer->fmt.fmt.yuv.mod != YUV_MOD_NON_MB_PLANAR)
        {
            __wrn("not support format!");
            fclose(fd);
            return EPDK_FAIL;
        }

        file_header.pix_fmt = framebuffer->fmt.fmt.yuv.pixelfmt;
        file_header.pix_seq = framebuffer->fmt.fmt.yuv.yuvseq;
        file_header.mod_or_br_swap = framebuffer->fmt.fmt.yuv.mod;
        file_header.data_len[0] = file_header.height * file_header.width;

        switch (framebuffer->fmt.fmt.yuv.pixelfmt)
        {
            case PIXEL_YUV444:
            {
                file_header.data_len[1] = file_header.height * file_header.width;
                file_header.data_len[2] = file_header.height * file_header.width;
                break;
            }

            case PIXEL_YUV422:
            {
                file_header.data_len[1] = file_header.height * file_header.width / 2;
                file_header.data_len[2] = file_header.height * file_header.width / 2;
                break;
            }

            case PIXEL_YUV420:
            {
                file_header.data_len[1] = file_header.height * file_header.width / 4;
                file_header.data_len[2] = file_header.height * file_header.width / 4;
                break;
            }

            case PIXEL_YUV411:
            {
                file_header.data_len[1] = file_header.height * file_header.width / 4;
                file_header.data_len[2] = file_header.height * file_header.width / 4;
                break;
            }

            default:
            {
                __wrn("not support format!");
                fclose(fd);
                return EPDK_FAIL;
            }
        }
    }

    file_header.offset_data[0] = sizeof(__fb_file_header_t);
    file_header.offset_data[1] = file_header.offset_data[0] + file_header.data_len[0];
    file_header.offset_data[2] = file_header.offset_data[1] + file_header.data_len[1];
    write_num = fwrite(&file_header, 1, sizeof(file_header), fd);

    if (write_num != sizeof(file_header))
    {
        __log("not enough space...");
        fclose(fd);
        return EPDK_FAIL;
    }

    write_num = fwrite(framebuffer->addr[0], 1, file_header.data_len[0], fd);

    if (write_num != file_header.data_len[0])
    {
        __log("not enough space...");
        fclose(fd);
        return EPDK_FAIL;
    }

    if (file_header.data_len[1] != 0)
    {
        write_num = fwrite(framebuffer->addr[1], 1, file_header.data_len[1], fd);

        if (write_num != file_header.data_len[1])
        {
            __log("not enough space...");
            fclose(fd);
            return EPDK_FAIL;
        }
    }

    if (file_header.data_len[2] != 0)
    {
        write_num = fwrite(framebuffer->addr[2], 1, file_header.data_len[2], fd);

        if (write_num != file_header.data_len[2])
        {
            __log("not enough space...");
            fclose(fd);
            return EPDK_FAIL;
        }
    }

    fclose(fd);
    return EPDK_OK;
}

#ifdef AOA_WALLPAPER_ENTER
static  stJpegFrameInfo *jpeg_info;
#define ALIGN_TO_16B(x)   ((((x) + (1 <<  4) - 1) >>  4) <<  4)
#define ALIGN_TO_32B(x)   ((((x) + (1 <<  5) - 1) >>  5) <<  5)

typedef struct
{
    __u32 width;
    __u32 height;
} JPG_SIZE;

static __s32 get_aoa_jpg_size(JPG_SIZE *size, __u8*dbuff)
{
    char mark[4] = {0};
    __u32 offset;
	__u32 i=0;
    //eLIBs_fread(mark, 1, 2, fp);
   // eLIBs_memcpy(mark, dbuff, 2);
   	mark[0] = dbuff[i];
	i++;
	mark[1] = dbuff[i];
	i++;
    if (!(mark[0] == 0xFF && mark[1] == 0xD8))
    {
        __testmsg("get jpg size fail!, (%x, %x)\n", mark[0], mark[1]);
        //eLIBs_fclose(fp);
        //fp = NULL;
        return -1;
    }

    mark[0] = 0;
    mark[1] = 0;
    //eLIBs_fread(mark, 1, 2, fp);
	//eLIBs_memcpy(mark, dbuff, 2);	
	mark[0] = dbuff[i];
	i++;
	mark[1] = dbuff[i];
	i++;
	 __testmsg("mark[0]=%x,mark[1]=%x\n", mark[0], mark[1]);
    while (mark[0] == 0xFF)
    {
        mark[0] = 0;
        mark[1] = 0;
		
	mark[0] = dbuff[i];
	i++;
	mark[1] = dbuff[i];
	i++;
	 //__testmsg("(%x, %x)\n", mark[0], mark[1]);
        //eLIBs_fread(mark, 1, 2, fp);
        offset = ((mark[1]) | (mark[0] << 8));
	 //__testmsg("offset=%d\n", offset);	
	i=i+offset-2;	
	
        // __testmsg("i=%d\n", i);
        //eLIBs_fseek(fp, offset - 2, SEEK_CUR);
        mark[0] = 0;
        mark[1] = 0;
	mark[0] = dbuff[i];
	i++;
	mark[1] = dbuff[i];
	i++;	
	// __testmsg("(%x, %x)\n", mark[0], mark[1]);
        //eLIBs_fread(mark, 1, 2, fp);
        if (mark[1] == 0xC0 || mark[1] == 0xC2)
        {
            __u8 temp[4] = {0};
		temp[0] = dbuff[i];
		i++;
		temp[1] = dbuff[i];
		i++;	
		
		temp[0] = dbuff[i];
		i++;
		
		temp[0] = dbuff[i];
		i++;
		temp[1] = dbuff[i];
		i++;	
            //eLIBs_memcpy(temp, dbuff, 2);
	   //eLIBs_memcpy(temp, dbuff, 1);
            //eLIBs_memcpy(temp, dbuff, 2);//eLIBs_fread(temp, 1, 2, fp);//height
           	 size->height = ((temp[1]) | (temp[0] << 8));
		temp[0] = dbuff[i];
		i++;
		temp[1] = dbuff[i];
		i++;		
            //eLIBs_memcpy(temp, dbuff, 2);//eLIBs_fread(temp, 1, 2, fp);//width
            size->width = ((temp[1]) | (temp[0] << 8));
		__testmsg("get jpg size  (%d, %d)\n", size->height, size->width);	
            break;
        }
    }
    return 0;
}

static int bound_pixel(int val)
{
	if (val >= 0 && val <= 255)
		return val;
	else
		return (val < 0) ? 0 : 255;
}

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
		
			//B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			//G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			//R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  

			R = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			B = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
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
	/*????????YUV444??16??,?????????,?????????*/
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

			//dstChartAddr[dst_row + j].b = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			//dstChartAddr[dst_row + j].g = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			//dstChartAddr[dst_row + j].r = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
			//dstChartAddr[dst_row + j].a = 255;

			B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
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



#endif



#ifdef CONFIG_BACKGRD_JPG
static  stJpegFrameInfo *jpeg_info;
#define ALIGN_TO_16B(x)   ((((x) + (1 <<  4) - 1) >>  4) <<  4)
#define ALIGN_TO_32B(x)   ((((x) + (1 <<  5) - 1) >>  5) <<  5)

typedef struct
{
    __u32 width;
    __u32 height;
} JPG_SIZE;

static __s32 get_jpg_size(JPG_SIZE *size, ES_FILE *fp)
{
    char mark[4] = {0};
    __u32 offset;
    eLIBs_fread(mark, 1, 2, fp);
    if (!(mark[0] == 0xFF && mark[1] == 0xD8))
    {
        __log("get jpg size fail!, (%x, %x)", mark[0], mark[1]);
        //eLIBs_fclose(fp);
        //fp = NULL;
        return -1;
    }

    mark[0] = 0;
    mark[1] = 0;
    eLIBs_fread(mark, 1, 2, fp);


    while (mark[0] == 0xFF)
    {
        mark[0] = 0;
        mark[1] = 0;
        eLIBs_fread(mark, 1, 2, fp);
        offset = ((mark[1]) | (mark[0] << 8));
        eLIBs_fseek(fp, offset - 2, SEEK_CUR);
        mark[0] = 0;
        mark[1] = 0;
        eLIBs_fread(mark, 1, 2, fp);
        if (mark[1] == 0xC0 || mark[1] == 0xC2)
        {
            __u8 temp[4] = {0};
            eLIBs_fread(temp, 1, 2, fp);//length
            eLIBs_fread(temp, 1, 1, fp);//data_precision
            eLIBs_fread(temp, 1, 2, fp);//height
            size->height = ((temp[1]) | (temp[0] << 8));
            eLIBs_fread(temp, 1, 2, fp);//width
            size->width = ((temp[1]) | (temp[0] << 8));
            break;
        }
    }
    return 0;
}

static int bound_pixel(int val)
{
	if (val >= 0 && val <= 255)
		return val;
	else
		return (val < 0) ? 0 : 255;
}

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
		
			//B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			//G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			//R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  

			R = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			B = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
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
	/*????????YUV444??16??,?????????,?????????*/
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

			//dstChartAddr[dst_row + j].b = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			//dstChartAddr[dst_row + j].g = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			//dstChartAddr[dst_row + j].r = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
			//dstChartAddr[dst_row + j].a = 255;

			B = bound_pixel(((yData[src_Y_idx] << 20) + Alpha1 *v_diff) >> 20); // b???  
			G = bound_pixel(((yData[src_Y_idx] << 20) - Alpha2_1 *u_diff - Alpha2_2 *v_diff) >> 20); // g???  
			R = bound_pixel(((yData[src_Y_idx] << 20) + Alhpa3 *u_diff) >> 20); // r???  
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



#endif




#if 0
/************************************************************************
* Function: convert_fb_yuv420
* Description: 将framebuffer转换为yuv420格式
* Input:
*    FB *in_frame：源frame buffer
*    FB *out_frame：目标frame buffer
*    SIZE *screen_size: 屏幕尺寸
*    uint8_t mod: 模式包括:
*       BACKLAYER_MOD_RATIO----适合屏幕尺寸(图片长宽比例不变)
*       BACKLAYER_MOD_STRETCH----拉伸模式,缩放到屏幕尺寸
* Output:
* Return:
*     EPDK_OK: 成功
*     EPDK_FAIL: 失败
*************************************************************************/
int32_t convert_fb(FB *in_frame, FB *out_frame, SIZE *screen_size, uint8_t mod)
{
    int32_t 				scaler_hdle;
    ES_FILE 				*de_hdle;
    __disp_scaler_para_t 	param;
    __disp_fb_t         	disp_in_frame;
    __disp_fb_t         	disp_out_frame;
    int32_t					ret_val = EPDK_OK;
    uint32_t 				arg[3];

    //  if (mod == BACKLAYER_MOD_RATIO)
    {
        out_frame->size = in_frame->size ;

        if (out_frame->size.width > screen_size->width)
        {
            out_frame->size.width = screen_size->width;
            out_frame->size.height = out_frame->size.width * in_frame->size.height / in_frame->size.width;
        }

        if (out_frame->size.height > screen_size->height)
        {
            out_frame->size.height = screen_size->height;
            out_frame->size.width = out_frame->size.height * in_frame->size.width / in_frame->size.height;
        }

        if (out_frame->size.width == 0)
        {
            out_frame->size.width = 1;
        }

        if (out_frame->size.height == 0)
        {
            out_frame->size.height = 1;
        }
    }

    //  else
    //  {
    //      out_frame->size = *screen_size;
    //  }
    /*由于带宽问题，最大尺寸为1280*720，大于这个尺寸由scaler mode放大得到*/
    if (out_frame->size.width > 1280)
    {
        out_frame->size.width = 1280;
        out_frame->size.height = out_frame->size.width * in_frame->size.height / in_frame->size.width;
    }

    if (out_frame->size.height > 720)
    {
        out_frame->size.height = 720;
        out_frame->size.width = out_frame->size.height * in_frame->size.width / in_frame->size.height;
    }

    if (out_frame->size.width > SCALE_OUT_MAX_WIDTH)
    {
        out_frame->size.width = SCALE_OUT_MAX_WIDTH;
    }

    if (out_frame->size.height > SCALE_OUT_MAX_HEIGHT)
    {
        out_frame->size.height = SCALE_OUT_MAX_HEIGHT;
    }

    if (in_frame->fmt.type == FB_TYPE_RGB)
    {
        __err("not support fb type!");
        return EPDK_FAIL;
    }

    out_frame->size.height = ((out_frame->size.height + 3) >> 2) << 2;
    out_frame->size.width = ((out_frame->size.width + 3) >> 2) << 2;

    if (out_frame->fmt.type != FB_TYPE_YUV && out_frame->fmt.type != FB_TYPE_RGB)
    {
        out_frame->fmt.type = FB_TYPE_YUV;
    }

    out_frame->fmt.cs_mode = in_frame->fmt.cs_mode;

    if (out_frame->fmt.type == FB_TYPE_YUV)
    {
#if 0
        {
            ES_FILE *pfile ;
            esKRNL_TimeDly(2000);
            pfile = eLIBs_fopen("f:\\bg_src_y", "w+");
            eLIBs_fwrite(in_frame->addr[0], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
            pfile = eLIBs_fopen("f:\\bg_src_u", "w+");
            eLIBs_fwrite(in_frame->addr[1], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
            pfile = eLIBs_fopen("f:\\bg_src_v", "w+");
            eLIBs_fwrite(in_frame->addr[2], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
        }
#endif
		uint32_t tmplen;
        if(gscene_bgd_get_flag() == EPDK_TRUE)  //默认背景 -- yuv444数据转换成yuv422
        {
            out_frame->fmt.fmt.yuv.mod = YUV_MOD_NON_MB_PLANAR;
            out_frame->fmt.fmt.yuv.pixelfmt = PIXEL_YUV422;
            out_frame->fmt.fmt.yuv.yuvseq = YUV_SEQ_OTHRS;
            tmplen = out_frame->size.height * out_frame->size.width;
            tmplen = ((tmplen >> 6) + 1) << 6; //64位对齐
            out_frame->addr[1] = (uint8_t *)out_frame->addr[0] + tmplen;
            out_frame->addr[2] = (uint8_t *)out_frame->addr[1] + tmplen / 2;

            memcpy(out_frame->addr[0], in_frame->addr[0], tmplen);
            // __log("yuv444 to yuv22 start tick:%d", esKRNL_TimeGet());
            {
				// UV444->UV422
                uint32_t   i, j ;
                char *pdst, *psrc ;
                psrc = in_frame->addr[1] ;
                pdst = out_frame->addr[1] ;

                for (i = 0; i < in_frame->size.height ; i++)
                {
                    for (j = 0; j < in_frame->size.width ; j += 2)
                    {
                        *pdst++ = *psrc++ ;
                        psrc++ ;
                    }
                }
            }
        	//memcpy(out_frame->addr[1],in_frame->addr[1],tmplen/2);
            {
				uint32_t   i, j ;
                char *pdst, *psrc ;
                psrc = in_frame->addr[2];
                pdst = out_frame->addr[2];

                for (i = 0; i < in_frame->size.height ; i++)
                {
                    for (j = 0; j < in_frame->size.width ; j += 2)
                    {
                        *pdst++ = *psrc++ ;
                        psrc++ ;
                    }
                }
            }
			//memcpy(out_frame->addr[2],in_frame->addr[2],tmplen/2);
            // __log("yuv444 to yuv22 finish tick:%d", esKRNL_TimeGet());
        }
        else if(gscene_bgd_get_flag() == EPDK_FALSE)  //自定义背景 -- yuv420数据不需要做转换
        {
            out_frame->fmt.fmt.yuv.mod = YUV_MOD_NON_MB_PLANAR;
            out_frame->fmt.fmt.yuv.pixelfmt = PIXEL_YUV420;
            out_frame->fmt.fmt.yuv.yuvseq = YUV_SEQ_UVUV;
            tmplen = out_frame->size.height * out_frame->size.width;
            tmplen = ((tmplen >> 6) + 1) << 6; //64位对齐
            out_frame->addr[1] = (uint8_t *)out_frame->addr[0] + tmplen;
            out_frame->addr[2] = (uint8_t *)out_frame->addr[1] + tmplen / 4;

            memcpy(out_frame->addr[0], in_frame->addr[0], tmplen);
            memcpy(out_frame->addr[1], in_frame->addr[1], tmplen/4);
            memcpy(out_frame->addr[2], in_frame->addr[2], tmplen/4);  
        }
#if 0
        {
            ES_FILE *pfile ;
            pfile = eLIBs_fopen("f:\\bg_dst_y", "w+");
            eLIBs_fwrite(out_frame->addr[0], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
            pfile = eLIBs_fopen("f:\\bg_dst_u", "w+");
            eLIBs_fwrite(out_frame->addr[1], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
            pfile = eLIBs_fopen("f:\\bg_dst_v", "w+");
            eLIBs_fwrite(out_frame->addr[2], 1, in_frame->size.width * in_frame->size.height, pfile);
            eLIBs_fclose(pfile);
        }
#endif
    }
    else
    {
        out_frame->fmt.fmt.rgb.pixelfmt = PIXEL_COLOR_ARGB8888;
        out_frame->fmt.fmt.rgb.br_swap = 0;
        out_frame->fmt.fmt.rgb.pixseq = 0;
    }

    return ret_val;
}

int32_t get_fb_from_file_data(uint8_t *buffer, uint8_t *filename)
{
    __hdle              fd;
    int32_t             ret_val = EPDK_FAIL;
    char                *pbuf;
    uint32_t            file_len;
    uint32_t            read_len;
    char                *uncompress_buf;
    uint32_t            uncompress_len;

    eLIBs_printf("filename=%s\n", filename);

    fd = eLIBs_fopen((char *)filename, "rb");

    if (fd == NULL)
    {
        eLIBs_printf("open file:%s err!\n", filename);
        return EPDK_FAIL;
    }

    eLIBs_fseek(fd, 0, SEEK_END);
    file_len    = eLIBs_ftell(fd);
    pbuf        = palloc((file_len + 1023) / 1024, 0);

    if (!pbuf)
    {
        eLIBs_fclose(fd);
        eLIBs_printf("palloc fail...\n");
        return EPDK_FAIL;
    }

    eLIBs_fseek(fd, 0, SEEK_SET);

    read_len    = eLIBs_fread(pbuf, 1, file_len, fd);

    if (read_len != file_len)
    {
        pfree(pbuf, (file_len + 1023) / 1024);
        eLIBs_fclose(fd);
        eLIBs_printf("fread fail...\n");
        return EPDK_FAIL;
    }

    //未压缩的背景
    if (EPDK_FALSE == AZ100_IsCompress(pbuf, file_len))
    {
        eLIBs_printf("bg pic is uncompress...\n");
        #if 1
        memcpy(pbuf, buffer, file_len);
		#else
        uncompress_buf  = pbuf;
        uncompress_len  = file_len;
        eLIBs_printf("uncompress_buf=%x", uncompress_buf);
        #endif
    }
    else//带压缩的背景
    {
        eLIBs_printf("bg pic is compress...\n");
        uncompress_len  = AZ100_GetUncompressSize(pbuf, file_len);
        #if 1
		uncompress_buf = buffer;
		#else
        uncompress_buf  = palloc((uncompress_len + 1023) / 1024, 0);
        #endif
        eLIBs_printf("compress_buf=%x\n", uncompress_buf);

        if (!uncompress_buf)
        {
            pfree(pbuf, (file_len + 1023) / 1024);
            eLIBs_fclose(fd);
            eLIBs_printf("palloc fail...\n");
            return EPDK_FAIL;
        }

        ret_val = AZ100_Uncompress(pbuf, file_len, uncompress_buf, uncompress_len);

        if (EPDK_FAIL == ret_val)
        {
            pfree(pbuf, (file_len + 1023) / 1024);
            pfree(uncompress_buf, (uncompress_len + 1023) / 1024);
            eLIBs_fclose(fd);
            eLIBs_printf("uncompress fail...\n");
            return EPDK_FAIL;
        }
        //memcpy(buffer, uncompress_buf, uncompress_len);
        pfree(pbuf, (file_len + 1023) / 1024);
        pbuf = NULL;
    }


EXIT_GET_FB0:
    eLIBs_fclose(fd);
	#if 0
    pfree(uncompress_buf, (uncompress_len + 1023) / 1024);
	#endif
    return ret_val;
}

/************************************************************************
* Function: get_fb_from_file
* Description: 从文件中取出fb
* Input:
*    FB *framebuffer：frame buffer.
*            设置输入的fb type为FB_TYPE_RGB或者FB_TYPE_YUV
*    uint8_t *buffer: framebuffer空间。默认为获取yuv422格式，大小为2*height*width
*    uint8_t *screen_size: 屏幕尺寸
*    uint8_t mod: 模式包括:
*       BACKLAYER_MOD_RATIO----适合屏幕尺寸(图片长宽比例不变)
*       BACKLAYER_MOD_STRETCH----拉伸模式,缩放到屏幕尺寸
*    uint8_t * filename：保存fb的文件名
* Output:
*    FB *framebuffer：读取到的的frame buffer
* Return:
*     EPDK_OK: 成功
*     EPDK_FAIL: 失败
*************************************************************************/
int32_t get_fb_from_file(FB *framebuffer, uint8_t *buffer, SIZE *screen_size, uint8_t mod, uint8_t *filename)
{
    __hdle              fd;
    uint32_t            tmp_len;
    __fb_file_header_t  file_header;
    int32_t             ret_val = EPDK_FAIL;
    FB                  tmp_frame;
    char                *pbuf;
    uint32_t            file_len;
    uint32_t            read_len;
    char                *uncompress_buf;
    uint32_t            uncompress_len;

    __log("filename=%s", filename);

    fd = eLIBs_fopen((char *)filename, "rb");

    if (fd == NULL)
    {
        __wrn("open file:%s err!", filename);
        return EPDK_FAIL;
    }

    eLIBs_fseek(fd, 0, SEEK_END);
    file_len    = eLIBs_ftell(fd);
    pbuf        = palloc((file_len + 1023) / 1024, 0);

    if (!pbuf)
    {
        eLIBs_fclose(fd);
        __log("palloc fail...");
        return EPDK_FAIL;
    }

    eLIBs_fseek(fd, 0, SEEK_SET);
    read_len    = eLIBs_fread(pbuf, 1, file_len, fd);

    if (read_len != file_len)
    {
        pfree(pbuf, (file_len + 1023) / 1024);
        eLIBs_fclose(fd);
        __log("fread fail...");
        return EPDK_FAIL;
    }

    //未压缩的背景
    if (EPDK_FALSE == AZ100_IsCompress(pbuf, file_len))
    {
        __log("bg pic is uncompress...");
        uncompress_buf  = pbuf;
        uncompress_len  = file_len;
        __log("uncompress_buf=%x", uncompress_buf);
    }
    else//带压缩的背景
    {
        __log("bg pic is compress...");
        uncompress_len  = AZ100_GetUncompressSize(pbuf, file_len);
        uncompress_buf  = palloc((uncompress_len + 1023) / 1024, 0);
        __log("compress_buf=%x", uncompress_buf);

        if (!uncompress_buf)
        {
            pfree(pbuf, (file_len + 1023) / 1024);
            eLIBs_fclose(fd);
            __log("palloc fail...");
            return EPDK_FAIL;
        }

        ret_val = AZ100_Uncompress(pbuf, file_len, uncompress_buf, uncompress_len);

        if (EPDK_FAIL == ret_val)
        {
            pfree(pbuf, (file_len + 1023) / 1024);
            pfree(uncompress_buf, (uncompress_len + 1023) / 1024);
            eLIBs_fclose(fd);
            __log("uncompress fail...");
            return EPDK_FAIL;
        }

        pfree(pbuf, (file_len + 1023) / 1024);
        pbuf = NULL;
    }

    pbuf    = uncompress_buf;
    memcpy((void *)&file_header, pbuf, sizeof(__fb_file_header_t));
    
    pbuf    += sizeof(__fb_file_header_t);
    memset(&tmp_frame, 0x0, sizeof(FB));

    tmp_frame.size.height   = file_header.height;
    tmp_frame.size.width    = file_header.width;
    tmp_frame.fmt.cs_mode   = (__cs_mode_t)file_header.cs_mode;
    tmp_frame.fmt.type      = (__fb_type_t)file_header.fmt_type;

    if (tmp_frame.fmt.type == FB_TYPE_RGB)
    {
    	__log("tmp_frame.fmt.type == FB_TYPE_RGB");
        tmp_frame.fmt.fmt.rgb.pixelfmt  = (__pixel_rgbfmt_t)file_header.pix_fmt;
        tmp_frame.fmt.fmt.rgb.pixseq    = (uint8_t)file_header.pix_seq;
        tmp_frame.fmt.fmt.rgb.br_swap   = (__bool)file_header.mod_or_br_swap;
    }
    else
    {
        if (file_header.mod_or_br_swap != YUV_MOD_NON_MB_PLANAR)
        {
            __wrn("not support format!");
            goto EXIT_GET_FB0;
        }

        tmp_frame.fmt.fmt.yuv.pixelfmt  = (__pixel_yuvfmt_t)file_header.pix_fmt;
        tmp_frame.fmt.fmt.yuv.mod       = (__yuv_mod_t)file_header.mod_or_br_swap;
        tmp_frame.fmt.fmt.yuv.pixelfmt  = (__pixel_yuvfmt_t)file_header.pix_fmt;
        tmp_frame.fmt.fmt.yuv.yuvseq    = (__yuv_seq_t)file_header.pix_seq;
    }

    tmp_frame.addr[0]   = pbuf;
    pbuf    +=  file_header.data_len[0];

    if (file_header.data_len[1] != 0)
    {
        tmp_frame.addr[1]   = pbuf;
        pbuf    +=  file_header.data_len[1];
    }

    if (file_header.data_len[2] != 0)
    {
        tmp_frame.addr[2]   = pbuf;
    }
/*	
if(1)
{
	static int cnt = 0;
	ES_FILE *yfile;
	ES_FILE *ufile;
	ES_FILE *vfile;
	char filenamey[20] ; 
	char filenameu[20] ; 
	char filenamev[20] ; 
		eLIBs_sprintf(filenamey, "E:\\y%d_.bin",cnt);
		eLIBs_sprintf(filenameu, "E:\\u%d_.bin",cnt);
		eLIBs_sprintf(filenamev, "E:\\v%d_.bin",cnt);
		cnt++;
	__err("filenamey = %s\n",filenamey);
	__err("filenameu = %s\n",filenameu);
	__err("filenamev = %s\n",filenamev);
	yfile=eLIBs_fopen(filenamey,"wb");
	ufile=eLIBs_fopen(filenameu,"wb");
	vfile=eLIBs_fopen(filenamev,"wb");
	if(yfile && ufile && vfile)
	{
	  __err("Create y.dat successed\n");      
	  __err("Create u.dat successed\n");      
	  __err("Create v.dat successed\n");      
	  eLIBs_fwrite((void *)tmp_frame.addr[0],1,640 * 480,yfile);
	  eLIBs_fwrite((void *)tmp_frame.addr[1],1,640 * 480/2,ufile);
	  eLIBs_fwrite((void *)tmp_frame.addr[2],1,640 * 480/2,vfile);
	  eLIBs_fclose(yfile);        
	  eLIBs_fclose(ufile);        
	  eLIBs_fclose(vfile);        
	}

}
*/
    framebuffer->addr[0]    = buffer;

    ret_val = convert_fb(&tmp_frame, framebuffer, screen_size, mod);

	//while(1)
		//esKRNL_TimeDly(100);
EXIT_GET_FB0:
    eLIBs_fclose(fd);

    pfree(uncompress_buf, (uncompress_len + 1023) / 1024);

    return ret_val;
}
#endif

__s32 get_fb_from_file_bmp (FB *framebuffer, __u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * filename)
{
	__u8 open_bmp_count=0;
    HBMP_i_t hbmp_buf;
    HBMP  hbmp = NULL;
    
    eLIBs_memset(&hbmp_buf, 0, sizeof(HBMP_i_t));
	__msg("filename=%s\n",filename);

	do
	{
    	hbmp = open_bmp(filename, &hbmp_buf);
		open_bmp_count++;
		if(hbmp == NULL){
			eLIBs_printf("ERR: open_bmp failed count:%d\n",open_bmp_count);
			}
		
	}while(hbmp == NULL && open_bmp_count<5);

    if(hbmp == NULL){
        __wrn("ERR: open_bmp failed\n");
        return -1;
    }
	
	framebuffer->size.width = get_width(hbmp);
	framebuffer->size.height =  get_height(hbmp);

	//PictureInfo->RowSize  = get_rowsize(hbmp);

	//PictureInfo->BufferSize = PictureInfo->RowSize * PictureInfo->Height;
	framebuffer->addr[0] = buffer;
	
	if(framebuffer->addr[0] == NULL){
		__wrn("ERR: wboot_malloc failed\n");
        goto error;
	}

	//eLIBs_memset(PictureInfo->Buffer, 0, PictureInfo->BufferSize);

	framebuffer->fmt.fmt.rgb.pixelfmt = PIXEL_COLOR_ARGB8888;
       framebuffer->fmt.fmt.rgb.br_swap = 0;
       framebuffer->fmt.fmt.rgb.pixseq = 0;
	get_matrix(hbmp, framebuffer->addr[0]);
	__msg("get bmp fb: w = %d, h= %d \n",framebuffer->size.width,framebuffer->size.height);
	close_bmp(hbmp);
	hbmp = NULL;

    return 0;

error:
	close_bmp(hbmp);
	
	return -1;
}





__s32 get_buffer_from_file_bmp (__u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * filename)
{
    HBMP_i_t hbmp_buf;
    HBMP  hbmp = NULL;
    
    eLIBs_memset(&hbmp_buf, 0, sizeof(HBMP_i_t));
    hbmp = open_bmp(filename, &hbmp_buf);
    if(hbmp == NULL){
        __wrn("ERR: open_bmp failed\n");
        return -1;
    }
	get_matrix(hbmp, buffer);
	close_bmp(hbmp);
	hbmp = NULL;

    return 0;

error:
	close_bmp(hbmp);
	
	return -1;
}


#ifdef USB_TINYJPEG_DEC
static void write_yuv(const char *filename, int width, int height, unsigned char **components)
{
	FILE *F;
	char temp[1024];

	snprintf(temp, 1024, "%s.YUV", filename);
	F = fopen(temp, "wb");

	if(F)
	{
		fwrite(components[0], 1, height*width, F);
		fwrite(components[1], 1, width*height / 4, F);
		fwrite(components[2], 1, width*height / 4, F);
		fclose(F);
	}
}

static void write_tga(const char *filename, int output_format, int width, int height, unsigned char **components)
{
	FILE *F;
	char temp[1024];
	unsigned int bufferlen = width * height * 3;
	unsigned char *rgb_data = components[0];

	snprintf(temp, sizeof(temp), filename);

	F = fopen(temp, "wb");
	if(F)
	{
		fwrite(rgb_data, 1, bufferlen, F);
		fclose(F);
	}
}




typedef struct _RGB888 {
	unsigned char r;
	unsigned char g;
	unsigned char b;
}  RGB888;



// 函数：RGB888转换为ARGB8888
void RGB888_to_ARGB8888(char *rgb24, char *argb32,int width,int high) {

	int i = 0;

	RGB888 *rgb = (RGB888 *)rgb24;
	ARGB8888 *argb = (ARGB8888 *)argb32;

	if(width == 0 || high == 0)
		return ;

	if(rgb24 == NULL || argb32 == NULL)
		return ;
		

	for(i=0;i<width*high;i++)
	{
	    argb[i].a = 255;  // Alpha值设为255，表示完全不透明
	    argb[i].r = rgb[i].r;
	    argb[i].g = rgb[i].g;
	    argb[i].b = rgb[i].b;
	}
}


int Tinyjpeg_dec(stJpegFrameInfo *jpeg_info,__u8 *buffer)
{
	unsigned int length_of_file;
	unsigned int width, height;
	unsigned char *buf;
	struct jdec_private *jdec;
	unsigned char *components[3];


	if(jpeg_info == NULL)
		return EPDK_FAIL;

	if(buffer == NULL)
		return EPDK_FAIL;
	

	length_of_file = jpeg_info->jpegData_len;

	if(length_of_file == NULL)
		return EPDK_FAIL;

	buf = (unsigned char *)jpeg_info->jpegData;
	if(buf == NULL)
		return EPDK_FAIL;

	
	/* Decompress it */
	jdec = tinyjpeg_init();
	if (jdec == NULL)
	{
		printf("Not enough memory to alloc the structure need for decompressing\n");
	}

	if (tinyjpeg_parse_header(jdec, buf, length_of_file) < 0)
	{
		printf(tinyjpeg_get_errorstring(jdec));
		return EPDK_FAIL;
	}

	/* Get the size of the image */
	tinyjpeg_get_size(jdec, &width, &height);
	jpeg_info->pic_width = width;
	jpeg_info->pic_height = height;

	if(width == 0 || height == 0)
		return EPDK_FAIL;
		

	printf("Decoding JPEG image w:%d,h:%d...\n",width,height);
	if (tinyjpeg_decode(jdec, TINYJPEG_FMT_RGB24) < 0)
	{
		printf(tinyjpeg_get_errorstring(jdec));
		return EPDK_FAIL;
	}

	/*
	 * Get address for each plane (not only max 3 planes is supported), and
	 * depending of the output mode, only some components will be filled
	 * RGB: 1 plane, YUV420P: 3 planes, GREY: 1 plane
	 */
	tinyjpeg_get_components(jdec, components);

	#if 1
	//write_yuv("f:\\wallpaper",width,height,components);
	write_tga("f:\\wallpaper.RGB",TINYJPEG_FMT_RGB24,width,height,components);
	#endif

	RGB888_to_ARGB8888((char *)components[0],(char *)buffer,width,height);


	/* Only called this if the buffers were allocated by tinyjpeg_decode() */
	tinyjpeg_free(jdec);
	/* else called just free(jdec); */
	printf("Decoding JPEG image succeed...\n");

	return 0;
}

#endif


#ifdef AOA_WALLPAPER_ENTER
__s32 get_fb_from_aoa_jpg (FB *framebuffer, __u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * dbuff)
{
    __disp_layer_info_t layer;
    JPG_SIZE *jpg_size;
    __u32 arg[3];
    ES_FILE *fp;
    __u32 file_size, tmpsize, disp_w, disp_h, aa, bb;
    __u32 yuv_size = 0;
    __s32 ret = -1;
    __u32 vcoder_mod_id;
    __mp *vcoder;
    static  __u32 release_flag = 0;
	reg_system_para_t *para;
	
	para = (reg_system_para_t *)dsk_reg_get_para_by_app(REG_APP_SYSTEM);

    memset(&layer, 0x00, sizeof(layer));
    jpeg_info = (stJpegFrameInfo *)esMEMS_Malloc(0, sizeof(stJpegFrameInfo));
     if (!jpeg_info)
    {
        __testmsg(" jpeg_info malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
    }

    memset(jpeg_info, 0x00, sizeof(stJpegFrameInfo));
    jpg_size = (JPG_SIZE *)esMEMS_Malloc(0, sizeof(JPG_SIZE));
    disp_w = screen_size->width;
    disp_h = screen_size->height;

   

    vcoder_mod_id =  esMODS_MInstall("d:\\mod\\vcoder.mod", 0);
    if (!vcoder_mod_id)
    {
        __testmsg("install vcoder failed...\n");
	ret = EPDK_FAIL;	
	goto error;
    }
    if (!vcoder)
    {
        vcoder = esMODS_MOpen(vcoder_mod_id, 0);

        if (!vcoder)
        {
            __testmsg("open mod ailed!\n");
		ret = EPDK_FAIL;	
		goto error;
        }
    }

   /* fp = eLIBs_fopen(filename, "rb");
    if (fp == NULL)
    {
        __log("open  file err!\n");
	ret = EPDK_FAIL;	
	goto error;
    }

    eLIBs_fseek(fp, 0, SEEK_END);
    file_size = eLIBs_ftell(fp);
    eLIBs_fseek(fp, 0, SEEK_SET);*/
    file_size = para->wallpap_lenth;
   __testmsg("file_size:%d\n", file_size);
    jpeg_info->jpegData = esMEMS_Malloc(0, file_size);
    if (jpeg_info->jpegData == NULL)
    {
        __testmsg("jpeg_info->jpegData malloc fail,file_size:%d\n", file_size);
	ret = EPDK_FAIL;	
	goto error;
    }
   /* tmpsize = eLIBs_fread(jpeg_info->jpegData, 1, file_size, fp);

    if (tmpsize != file_size)
    {
        __log("write file err! %d- %d\n", tmpsize, file_size);
	ret = EPDK_FAIL;	
	goto error;
    }
    jpeg_info->jpegData_len = tmpsize;
    eLIBs_fseek(fp, 0, SEEK_SET);*/
   	 eLIBs_memcpy(jpeg_info->jpegData, dbuff, file_size);	
	  __testmsg("file_size:%d\n", file_size);
	 jpeg_info->jpegData_len = file_size;
    if(get_aoa_jpg_size(jpg_size, &dbuff[0]) == -1)
	{
		ret = EPDK_FAIL;	
		goto error;
    }	
	__testmsg("get_aoa_jpg_size\n");


	if(jpg_size->width!=disp_w || jpg_size->height!=disp_h)
	{
		ret = EPDK_FAIL;	
		goto error;
	}

	

    yuv_size = ALIGN_TO_16B(jpg_size->width) * ALIGN_TO_16B(jpg_size->height) * 3 / 2;
    jpeg_info->yuv_buf = (__u32)esMEMS_Palloc(((yuv_size + 1023) >> 10), 0);
    eLIBs_CleanFlushDCacheRegion((void *)jpeg_info->yuv_buf, yuv_size);

    ret = esMODS_MIoctrl(vcoder, MPEJ_CODEC_CMD_DECODER, 0, jpeg_info);
	__testmsg("ret=%d\n",ret);
  
	
	framebuffer->size.width = jpeg_info->pic_width;
	framebuffer->size.height =	jpeg_info->pic_height;
	framebuffer->addr[0] = buffer;


	ret = YV12_Scalar_To_RGB32_aligned((__u8 *)jpeg_info->y_buf,(__u8 *)jpeg_info->u_buf,
										(__u8 *)jpeg_info->v_buf,(__u32 *)framebuffer->addr[0],
										jpeg_info->pic_width,jpeg_info->pic_height,
										jpeg_info->pic_width,jpeg_info->pic_height);
	
	
	__log("YV444_Scalar_To_RGB32_aligned:%d\n",ret);
	
	if(framebuffer->addr[0] == NULL){
		__wrn("ERR: wboot_malloc failed\n");
		ret = EPDK_FAIL;	
		goto error;
	}


	framebuffer->fmt.fmt.rgb.pixelfmt = PIXEL_COLOR_ARGB8888;
	   framebuffer->fmt.fmt.rgb.br_swap = 0;
	   framebuffer->fmt.fmt.rgb.pixseq = 0;

error:

	if(vcoder)
	{
    	 	esMODS_MClose(vcoder);
		vcoder = NULL;	
	}

	if(vcoder_mod_id)
	{
    		esMODS_MUninstall(vcoder_mod_id);
		vcoder_mod_id = NULL;	
	}

	if(fp)
	{
     		eLIBs_fclose(fp);
     		fp = NULL;
	}

	if(jpg_size)
	{
    		esMEMS_Mfree(0, jpg_size);
		jpg_size = NULL;	
	}

	if (jpeg_info)
	{
	    if (jpeg_info->jpegData)
	    {
	        esMEMS_Mfree(0, jpeg_info->jpegData);
	        jpeg_info->jpegData = NULL;
	    }
	    if (jpeg_info->yuv_buf)
	    {
	        esMEMS_Pfree(jpeg_info->yuv_buf, ((yuv_size + 1023) >> 10));
	        jpeg_info->yuv_buf = NULL;
	    }
	    esMEMS_Mfree(0, jpeg_info);
	    jpeg_info = NULL;
	}



	
	
	return ret;
}



__s32 get_buffer_from_aoa_jpg(__u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * dbuff)
{
	__disp_layer_info_t layer;
	JPG_SIZE *jpg_size=NULL;
	__u32 arg[3];
	ES_FILE *fp;
	__u32 file_size, tmpsize, disp_w, disp_h, aa, bb;
	__u32 yuv_size = 0;
	__s32 ret = -1;
	__u32 vcoder_mod_id;
	__mp *vcoder;
	static	__u32 release_flag = 0;

	reg_system_para_t *para;
	
	para = (reg_system_para_t *)dsk_reg_get_para_by_app(REG_APP_SYSTEM);
	memset(&layer, 0x00, sizeof(layer));
	jpeg_info = (stJpegFrameInfo *)esMEMS_Malloc(0, sizeof(stJpegFrameInfo));
	 if (!jpeg_info)
	{
		__log(" jpeg_info malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
	}
	memset(jpeg_info, 0x00, sizeof(stJpegFrameInfo));	
	
	jpg_size = (JPG_SIZE *)esMEMS_Malloc(0, sizeof(JPG_SIZE));
	disp_w = screen_size->width;
	disp_h = screen_size->height;

	 if (!jpg_size)
	{
		__log(" jpg_size malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
	}

   
#ifndef USB_TINYJPEG_DEC
	vcoder_mod_id = esMODS_MInstall("d:\\mod\\vcoder.mod", 0);
	if (!vcoder_mod_id)
	{
		__log("install vcoder failed...\n");
	ret = EPDK_FAIL;	
	goto error;
	}
	if (!vcoder)
	{
		vcoder = esMODS_MOpen(vcoder_mod_id, 0);

		if (!vcoder)
		{
			__log("open mod ailed!\n");
		ret = EPDK_FAIL;	
		goto error;
		}
	}
#endif
	/*fp = eLIBs_fopen(filename, "rb");
	if (fp == NULL)
	{
		__log("open  file err!\n");
	ret = EPDK_FAIL;	
	goto error;
	}

	eLIBs_fseek(fp, 0, SEEK_END);
	file_size = eLIBs_ftell(fp);
	eLIBs_fseek(fp, 0, SEEK_SET);
	jpeg_info->jpegData = esMEMS_Malloc(0, file_size);
	if (jpeg_info->jpegData == NULL)
	{
		__log("jpeg_info->jpegData malloc fail,file_size:%d\n", file_size);
	ret = EPDK_FAIL;	
	goto error;
	}
	tmpsize = eLIBs_fread(jpeg_info->jpegData, 1, file_size, fp);

	if (tmpsize != file_size)
	{
		__log("write file err! %d- %d\n", tmpsize, file_size);
	ret = EPDK_FAIL;	
	goto error;
	}
	jpeg_info->jpegData_len = tmpsize;
	eLIBs_fseek(fp, 0, SEEK_SET);

	get_aoa_jpg_size(jpg_size, fp);*/

	 file_size = para->wallpap_lenth;
   __testmsg("file_size:%d\n", file_size);
    jpeg_info->jpegData = esMEMS_Malloc(0, file_size);
    if (jpeg_info->jpegData == NULL)
    {
        __testmsg("jpeg_info->jpegData malloc fail,file_size:%d\n", file_size);
	ret = EPDK_FAIL;	
	goto error;
    }

   	 eLIBs_memcpy(jpeg_info->jpegData, dbuff, file_size);	
	  __testmsg("file_size:%d\n", file_size);
	 jpeg_info->jpegData_len = file_size;
	 
    	if(get_aoa_jpg_size(jpg_size, &dbuff[0]) == -1)
    	{
    		ret = EPDK_FAIL;	
			goto error;
    	}
	__testmsg("get_aoa_jpg_size\n");
	__testmsg("disp_w:%d,disp_h:%d\n", disp_w,disp_h);
	__testmsg("jpg_size->width:%d,jpg_size->height:%d\n", jpg_size->width,jpg_size->height);
	if(jpg_size->width!=disp_w || jpg_size->height!=disp_h)
	{
		ret = EPDK_FAIL;	
		goto error;
	}

	
	#ifdef USB_TINYJPEG_DEC
	yuv_size = jpg_size->width * jpg_size->height * 3;
	jpeg_info->yuv_buf = (__u32)esMEMS_Palloc(((yuv_size + 1023) >> 10), 0);
	//eLIBs_CleanFlushDCacheRegion((void *)jpeg_info->yuv_buf, yuv_size);
	__testmsg("jpeg_info->yuv_buf=0x%x, yuv_size=%d\n",jpeg_info->yuv_buf, yuv_size);
	ret = Tinyjpeg_dec(jpeg_info,buffer);
	#else
	yuv_size = ALIGN_TO_16B(jpg_size->width) * ALIGN_TO_16B(jpg_size->height) * 3 / 2;
	jpeg_info->yuv_buf = (__u32)esMEMS_Palloc(((yuv_size + 1023) >> 10), 0);
	eLIBs_CleanFlushDCacheRegion((void *)jpeg_info->yuv_buf, yuv_size);
	__testmsg("jpeg_info->yuv_buf=0x%x, yuv_size=%d\n",jpeg_info->yuv_buf, yuv_size);
	ret = esMODS_MIoctrl(vcoder, MPEJ_CODEC_CMD_DECODER, 0, jpeg_info);

	__testmsg("ret=%d\n",ret);

	if(ret == EPDK_FAIL)
	{
		ret = EPDK_FAIL;	
		goto error;
	}
	
	__testmsg("MPEJ_CODEC_CMD_DECODER ret:%d\n", ret);
	__testmsg("jpeg_info->pic_width:%d\n", jpeg_info->pic_width);
	__testmsg("jpeg_info->pic_height:%d\n", jpeg_info->pic_height);


	ret = YV12_Scalar_To_RGB32_aligned((__u8 *)jpeg_info->y_buf,(__u8 *)jpeg_info->u_buf,
										(__u8 *)jpeg_info->v_buf,(__u32 *)buffer,
										jpeg_info->pic_width,jpeg_info->pic_height,
										//800,480);
										jpeg_info->pic_width,jpeg_info->pic_height);
	
	__log("YV444_Scalar_To_RGB32_aligned:%d\n",ret);
	#endif
	

error:
#ifndef USB_TINYJPEG_DEC
	if(vcoder)
	{
			esMODS_MClose(vcoder);
		vcoder = NULL;	
	}

	if(vcoder_mod_id)
	{
			esMODS_MUninstall(vcoder_mod_id);
		vcoder_mod_id = NULL;	
	}
#endif	

	if(fp)
	{
			eLIBs_fclose(fp);
			fp = NULL;
	}

	if(jpg_size)
	{
			esMEMS_Mfree(0, jpg_size);
		jpg_size = NULL;	
	}

	if (jpeg_info)
	{
		if (jpeg_info->jpegData)
		{
			esMEMS_Mfree(0, jpeg_info->jpegData);
			jpeg_info->jpegData = NULL;
		}
		if (jpeg_info->yuv_buf)
		{
			esMEMS_Pfree(jpeg_info->yuv_buf, ((yuv_size + 1023) >> 10));
			jpeg_info->yuv_buf = NULL;
		}
		esMEMS_Mfree(0, jpeg_info);
		jpeg_info = NULL;
	}

	


	
	
	return ret;
}



#endif


#ifdef CONFIG_BACKGRD_JPG
__s32 get_fb_from_file_jpg (FB *framebuffer, __u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * filename)
{
    __disp_layer_info_t layer;
    JPG_SIZE *jpg_size;
    __u32 arg[3];
    ES_FILE *fp;
    __u32 file_size, tmpsize, disp_w, disp_h, aa, bb;
    __u32 yuv_size = 0;
    __s32 ret = -1;
    __u32 vcoder_mod_id;
    __mp *vcoder;
    static  __u32 release_flag = 0;

    memset(&layer, 0x00, sizeof(layer));
    jpeg_info = (stJpegFrameInfo *)esMEMS_Malloc(0, sizeof(stJpegFrameInfo));
     if (!jpeg_info)
    {
        __log(" jpeg_info malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
    }

	
    jpg_size = (JPG_SIZE *)esMEMS_Malloc(0, sizeof(JPG_SIZE));
    disp_w = screen_size->width;
    disp_h = screen_size->height;

   

    vcoder_mod_id =  esMODS_MInstall("d:\\mod\\vcoder.mod", 0);
    if (!vcoder_mod_id)
    {
        __log("install vcoder failed...\n");
	ret = EPDK_FAIL;	
	goto error;
    }
    if (!vcoder)
    {
        vcoder = esMODS_MOpen(vcoder_mod_id, 0);

        if (!vcoder)
        {
            __log("open mod ailed!\n");
		ret = EPDK_FAIL;	
		goto error;
        }
    }

    fp = eLIBs_fopen(filename, "rb");
    if (fp == NULL)
    {
        __log("open  file err!\n");
	ret = EPDK_FAIL;	
	goto error;
    }

    eLIBs_fseek(fp, 0, SEEK_END);
    file_size = eLIBs_ftell(fp);
    eLIBs_fseek(fp, 0, SEEK_SET);
    jpeg_info->jpegData = esMEMS_Malloc(0, file_size);
    if (jpeg_info->jpegData == NULL)
    {
        __log("jpeg_info->jpegData malloc fail,file_size:%d\n", file_size);
	ret = EPDK_FAIL;	
	goto error;
    }
    tmpsize = eLIBs_fread(jpeg_info->jpegData, 1, file_size, fp);

    if (tmpsize != file_size)
    {
        __log("write file err! %d- %d\n", tmpsize, file_size);
	ret = EPDK_FAIL;	
	goto error;
    }
    jpeg_info->jpegData_len = tmpsize;
    eLIBs_fseek(fp, 0, SEEK_SET);

    get_jpg_size(jpg_size, fp);

    yuv_size = ALIGN_TO_16B(jpg_size->width) * ALIGN_TO_16B(jpg_size->height) * 3 / 2;
    jpeg_info->yuv_buf = (__u32)esMEMS_Palloc(((yuv_size + 1023) >> 10), 0);
    eLIBs_CleanFlushDCacheRegion((void *)jpeg_info->yuv_buf, yuv_size);

    ret = esMODS_MIoctrl(vcoder, MPEJ_CODEC_CMD_DECODER, 0, jpeg_info);

  
	
	framebuffer->size.width = jpeg_info->pic_width;
	framebuffer->size.height =	jpeg_info->pic_height;
	framebuffer->addr[0] = buffer;


	ret = YV12_Scalar_To_RGB32_aligned((__u8 *)jpeg_info->y_buf,(__u8 *)jpeg_info->u_buf,
										(__u8 *)jpeg_info->v_buf,(__u32 *)framebuffer->addr[0],
										jpeg_info->pic_width,jpeg_info->pic_height,
										jpeg_info->pic_width,jpeg_info->pic_height);
	
	
	__log("YV444_Scalar_To_RGB32_aligned:%d\n",ret);
	
	if(framebuffer->addr[0] == NULL){
		__wrn("ERR: wboot_malloc failed\n");
		ret = EPDK_FAIL;	
		goto error;
	}


	framebuffer->fmt.fmt.rgb.pixelfmt = PIXEL_COLOR_ARGB8888;
	   framebuffer->fmt.fmt.rgb.br_swap = 0;
	   framebuffer->fmt.fmt.rgb.pixseq = 0;

error:
	if(vcoder)
	{
    	 	esMODS_MClose(vcoder);
		vcoder = NULL;	
	}

	if(vcoder_mod_id)
	{
    		esMODS_MUninstall(vcoder_mod_id);
		vcoder_mod_id = NULL;	
	}

	if(fp)
	{
     		eLIBs_fclose(fp);
     		fp = NULL;
	}

	if(jpg_size)
	{
    		esMEMS_Mfree(0, jpg_size);
		jpg_size = NULL;	
	}

	if (jpeg_info)
	{
	    if (jpeg_info->jpegData)
	    {
	        esMEMS_Mfree(0, jpeg_info->jpegData);
	        jpeg_info->jpegData = NULL;
	    }
	    if (jpeg_info->yuv_buf)
	    {
	        esMEMS_Pfree(jpeg_info->yuv_buf, ((yuv_size + 1023) >> 10));
	        jpeg_info->yuv_buf = NULL;
	    }
	    esMEMS_Mfree(0, jpeg_info);
	    jpeg_info = NULL;
	}

	


	
	
	return ret;
}

__s32 get_buffer_from_file_jpg (__u8 *buffer, SIZE *screen_size, __u8 mod, __u8 * filename)
{
	__disp_layer_info_t layer;
	JPG_SIZE *jpg_size=NULL;
	__u32 arg[3];
	ES_FILE *fp;
	__u32 file_size, tmpsize, disp_w, disp_h, aa, bb;
	__u32 yuv_size = 0;
	__s32 ret = -1;
	__u32 vcoder_mod_id;
	__mp *vcoder;
	static	__u32 release_flag = 0;

	memset(&layer, 0x00, sizeof(layer));
	jpeg_info = (stJpegFrameInfo *)esMEMS_Malloc(0, sizeof(stJpegFrameInfo));
	 if (!jpeg_info)
	{
		__log(" jpeg_info malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
	}

	
	jpg_size = (JPG_SIZE *)esMEMS_Malloc(0, sizeof(JPG_SIZE));
	disp_w = screen_size->width;
	disp_h = screen_size->height;

	 if (!jpg_size)
	{
		__log(" jpg_size malloc fail err");
	ret = EPDK_FAIL;	
	goto error;
	}

   

	vcoder_mod_id = esMODS_MInstall("d:\\mod\\vcoder.mod", 0);
	if (!vcoder_mod_id)
	{
		__log("install vcoder failed...\n");
	ret = EPDK_FAIL;	
	goto error;
	}
	if (!vcoder)
	{
		vcoder = esMODS_MOpen(vcoder_mod_id, 0);

		if (!vcoder)
		{
			__log("open mod ailed!\n");
		ret = EPDK_FAIL;	
		goto error;
		}
	}

	fp = eLIBs_fopen(filename, "rb");
	if (fp == NULL)
	{
		__log("open  file err!\n");
	ret = EPDK_FAIL;	
	goto error;
	}

	eLIBs_fseek(fp, 0, SEEK_END);
	file_size = eLIBs_ftell(fp);
	eLIBs_fseek(fp, 0, SEEK_SET);
	jpeg_info->jpegData = esMEMS_Malloc(0, file_size);
	if (jpeg_info->jpegData == NULL)
	{
		__log("jpeg_info->jpegData malloc fail,file_size:%d\n", file_size);
	ret = EPDK_FAIL;	
	goto error;
	}
	tmpsize = eLIBs_fread(jpeg_info->jpegData, 1, file_size, fp);

	if (tmpsize != file_size)
	{
		__log("write file err! %d- %d\n", tmpsize, file_size);
	ret = EPDK_FAIL;	
	goto error;
	}
	jpeg_info->jpegData_len = tmpsize;
	eLIBs_fseek(fp, 0, SEEK_SET);

	get_jpg_size(jpg_size, fp);

	yuv_size = ALIGN_TO_16B(jpg_size->width) * ALIGN_TO_16B(jpg_size->height) * 3 / 2;
	jpeg_info->yuv_buf = (__u32)esMEMS_Palloc(((yuv_size + 1023) >> 10), 0);
	eLIBs_CleanFlushDCacheRegion((void *)jpeg_info->yuv_buf, yuv_size);

	ret = esMODS_MIoctrl(vcoder, MPEJ_CODEC_CMD_DECODER, 0, jpeg_info);


	ret = YV12_Scalar_To_RGB32_aligned((__u8 *)jpeg_info->y_buf,(__u8 *)jpeg_info->u_buf,
										(__u8 *)jpeg_info->v_buf,(__u32 *)buffer,
										jpeg_info->pic_width,jpeg_info->pic_height,
										jpeg_info->pic_width,jpeg_info->pic_height);
	
	__log("YV444_Scalar_To_RGB32_aligned:%d\n",ret);

error:
	if(vcoder)
	{
			esMODS_MClose(vcoder);
		vcoder = NULL;	
	}

	if(vcoder_mod_id)
	{
			esMODS_MUninstall(vcoder_mod_id);
		vcoder_mod_id = NULL;	
	}

	if(fp)
	{
			eLIBs_fclose(fp);
			fp = NULL;
	}

	if(jpg_size)
	{
			esMEMS_Mfree(0, jpg_size);
		jpg_size = NULL;	
	}

	if (jpeg_info)
	{
		if (jpeg_info->jpegData)
		{
			esMEMS_Mfree(0, jpeg_info->jpegData);
			jpeg_info->jpegData = NULL;
		}
		if (jpeg_info->yuv_buf)
		{
			esMEMS_Pfree(jpeg_info->yuv_buf, ((yuv_size + 1023) >> 10));
			jpeg_info->yuv_buf = NULL;
		}
		esMEMS_Mfree(0, jpeg_info);
		jpeg_info = NULL;
	}

	


	
	
	return ret;
}
#endif










