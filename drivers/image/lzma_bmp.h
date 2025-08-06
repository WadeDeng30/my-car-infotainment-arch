/*
************************************************************************************************************************
*                                                        BITMAP
*
*                             Copyright(C), 2006-2009, SoftWinners Microelectronic Co., Ltd.
*											       All Rights Reserved
*
* File Name : bmp.h
*
* Author : Gary.Wang
*
* Version : 1.1.0
*
* Date : 2009.04.07
*
* Description : 
*
* Others : None at present.
*
*
* History :
*
*  <Author>        <time>       <version>      <description>
*
* Gary.Wang      2009.04.07       1.1.0        build the file
*
************************************************************************************************************************
*/
#ifndef  dis_lzma_bmp_h
#define  dis_lzma_bmp_h

#include <typedef.h>
#include <elibs_stdio.h>
#include <kconfig.h>
#include <sys_device.h>
#include <mod_defs.h>
#include <log.h>
#include <kapi.h>
#include <elibs_string.h>
#include <mod_mcu.h>
#include <libc.h>
#include <misc/pub0.h>
#include <string.h>
//#include <csp.h>
#include <libc/eLIBs_az100.h>
//#include "mod_orange.h"
//#include "mod_orange/orange_api_entry.h"
#include "desktop.h"


enum _origin_pos_e_lzma
{
	LZMA_ORIGIN_POS_UPPER_LEFT,
    LZMA_ORIGIN_POS_LOWER_LEFT
};


#define PALETTE_COLOR_BYTE_COUNT        4




typedef  struct _full_color_t_lzma
{
	__u8  B;
	__u8  G;
	__u8  R;
	__u8  A;
}__attribute__ ((packed)) full_color_t_lzma;


typedef  struct _bmp_file_head_t_lzma
{
	char    bfType[2];        // "BM"
	__u32   bfSize;           // total size of bmp file
	__u32   bfReserved;       // reserved field
	__u32   bfOffBits;        // pixel matrix offset from file head
}__attribute__ ((packed)) bmp_file_head_t_lzma;


typedef  struct _bmp_info_head_t_lzma
{
	__u32   biSize;           // the size of bmp information head
	__u32   biWidth;          // the width of bmp, unit: pixel
	__s32   biHeight;         // the height of bmp, unit:pixel
	__u16   biPlanes;         // always 1
	__u16   biBitCount;       // bits per pixel. 1-mono, 4-16color, 8-256color, 24-truecolor
	__u32   biCompression;    // copression format. 0: no compression.
	__u32   biSizeImage;      // the size of pixel matirx.
	__u32   biXPelsPerMeter;  // horizontal dis, unit : pixel/meter
	__u32   biYPelsPerMeter;  // vertical dis, unit : pixel/meter
	__u32   biClrUsed;        // number of used colors, 0 means 2^biBitCount
	__u32   biClrImportant;   // number of important colors, 0 means that all colors are important.
}__attribute__ ((packed)) bmp_info_head_t_lzma;


typedef struct _HBMP_i_t_lzma
{
	//__u32   byte_count;
	__u16   bitcount;
	__u32   width;
	__s32   height;
	__u32   row_size;
	__u32   matrix_off;
    __u32   real_row_size;//useful data row size
}HBMP_i_t_lzma, *HBMP_i_lzma;


typedef __u32  HBMP_lzma;
typedef unsigned long HTHEME;
typedef void *HRES;

typedef struct _theme_t
{
    __u32   id;
    __u32   size;//原始的大小
    __u32   size_com;//存储的大小
    void    *buf;
} theme_t, *HTHEME_i;


extern HBMP_lzma  lzma_open_bmp( __u8 *bmp_file, __u32 size,HBMP_i_t_lzma *hbmp_hdl_buf,__u8 mem);
extern __s32   lzma_close_bmp   ( HBMP_lzma hbmp );
extern __u32   lzma_get_bitcount( HBMP_lzma hbmp );
extern __u32   lzma_get_width   ( HBMP_lzma hbmp );
extern __s32   lzma_get_height  ( HBMP_lzma hbmp );
extern __u32   lzma_get_rowsize ( HBMP_lzma hbmp );
extern __s32   lzma_get_matrix  ( HBMP_lzma hbmp, void *buf );
extern __s32   lzma_read_pixel  ( HBMP_lzma hbmp, __u32 x, __u32 y, full_color_t_lzma *color_p, __s32 origin_pos );
extern __s32   lzma_read_index  ( HBMP_lzma hbmp, __u32 x, __u32 y, __u8 *index_p, __s32 origin_pos );
#if 0
extern __s32 dsk_theme_init(const char *file);
extern HTHEME dsk_theme_open(__u32 theme_id);
extern void *dsk_theme_hdl2buf(HTHEME handle);
#endif


#endif     //  ifndef __bmp_h

/* end of bmp.h  */

