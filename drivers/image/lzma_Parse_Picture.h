/*
*************************************************************************************
*                         			eGon
*					        Application Of eGon2.0
*
*				(c) Copyright 2006-2010, All winners Co,Ld.
*							All	Rights Reserved
*
* File Name 	: Parse_Picture.h
*
* Author 		: javen
*
* Description 	: 图片解析
*
* History 		:
*      <author>    		<time>       	<version >    		<desc>
*       javen     	   2010-09-10          1.0            create this file
*
*************************************************************************************
*/
#ifndef  __LZMA_PARSE_PICTURE_H__
#define  __LZMA_PARSE_PICTURE_H__

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
#include "lvgl.h"
#include  "lzma_bmp.h"

#ifndef I8
#define I8      signed char
#endif
#ifndef U8
#define U8      unsigned char     /* unsigned 8  bits. */
#endif
#ifndef I16
#define I16     signed short    /*   signed 16 bits. */
#endif
#ifndef U16
#define U16     unsigned short    /* unsigned 16 bits. */
#endif
#ifndef I32
#define I32     signed int   /*   signed 32 bits. */
#endif
#ifndef U32
#define U32     unsigned int   /* unsigned 32 bits. */
#endif
#define I16P    I16              /*   signed 16 bits OR MORE ! */
#define U16P    U16              /* unsigned 16 bits OR MORE ! */

typedef struct tag_Picture_lzma{
    void *Buffer;   		/* 存放图片数据     */
    __u32 BufferSize;   	/* buffer长度       */
    
    __u32 BitCount; 		/* 一个像素的bit数  */
    __u32 Width;    		/* 图片宽度         */
    __u32 Height;   		/* 图片高度         */
    __u32 RowSize;  		/* 图片一行的大小   */
}Picture_t_lzma;

__s32 Lzma_Parse_Pic_BMP_ByBuffer(void *Pic_Buffer, __u32 Pic_BufferSize, Picture_t_lzma *PictureInfo);
__s32 Lzma_Parse_Pic_BMP_ByPath(__u8 *Path, __u32 size, lv_img_dsc_t *PictureInfo, __u32 Addr,__u8 mem);
__s32 Lzma_get_theme(uint32_t theme_id, HTHEME	* bmp);
__s32 Lzma_get_theme_buf(uint32_t theme_id, HTHEME	* bmp, lv_img_dsc_t *icon);


#endif   //__LZMA_PARSE_PICTURE_H__

