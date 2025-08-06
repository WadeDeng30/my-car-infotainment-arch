/*
*************************************************************************************
*                         			eGon
*					        Application Of eGon2.0
*
*				(c) Copyright 2006-2010, All winners Co,Ld.
*							All	Rights Reserved
*
* File Name 	: Parse_Picture.c
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

//#include  "..\\Esh_shell_private.h"
#include  "lzma_Parse_Picture.h"
#include  "apps.h"
#define spcl_size_align( x, y )         ( ( (x) + (y) - 1 ) & ~( (y) - 1 ) )
#define abs(x) (x) >= 0 ? (x):-(x)

static uint32_t data_tatol = 0;

/*
*******************************************************************************
*                     Parse_Pic_BMP_ByBuffer
*
* Description:
*    解析存放在内存中的图片
*
* Parameters:
*    Pic_Buffer     :  input.  存放图片的入口
*    Pic_BufferSize :  input.  缓冲区大小
*    PictureInfo    :  output. 图片解析后的信息
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
__s32 Lzma_Parse_Pic_BMP_ByBuffer(void *Pic_Buffer, __u32 Pic_BufferSize, Picture_t_lzma *PictureInfo)
{
	bmp_info_head_t_lzma *info_head_p = NULL;

    info_head_p = (bmp_info_head_t_lzma *)((__u32)Pic_Buffer + sizeof(bmp_file_head_t_lzma));

    PictureInfo->BitCount = info_head_p->biBitCount;
	PictureInfo->Width    = info_head_p->biWidth;
	PictureInfo->Height	  = abs(info_head_p->biHeight);
	PictureInfo->RowSize  = spcl_size_align( info_head_p->biWidth * ( info_head_p->biBitCount >> 3 ), 4 );

	PictureInfo->Buffer     = (void *)((__u32)Pic_Buffer + sizeof(bmp_info_head_t_lzma) + sizeof(bmp_file_head_t_lzma));
	PictureInfo->BufferSize = Pic_BufferSize - (sizeof(bmp_info_head_t_lzma) + sizeof(bmp_file_head_t_lzma));

	return 0;
}

/*
*******************************************************************************
*                     Parse_Pic_BMP
*
* Description:
*    以路径名来解析图片, 并且把解析处理的图片拷贝到指定的地址，
* 如果指定的地址为NULL, 则可以存放在任何地址。
*
* Parameters:
*    Path           :  input.  图片路径
*    PictureInfo    :  output. 图片解析后的信息
*    Addr			:  input.  存放解析后的图片, 
*
* Return value:
*    void
*
* note:
*    void
*
*******************************************************************************
*/
__s32 Lzma_Parse_Pic_BMP_ByPath(__u8 *Path, __u32 size, lv_img_dsc_t *PictureInfo, __u32 Addr,__u8 mem)
{
    HBMP_i_t_lzma hbmp_buf;
    HBMP_lzma  hbmp = NULL;
    
    eLIBs_memset(&hbmp_buf, 0, sizeof(HBMP_i_t_lzma));
    hbmp = lzma_open_bmp(Path,size, &hbmp_buf,mem);
    if(hbmp == NULL){
        eLIBs_printf("ERR: open_bmp failed\n");
        return -1;
    }

	PictureInfo->header.always_zero = 0;
	PictureInfo->header.w = lzma_get_width(hbmp);
	PictureInfo->header.h = lzma_get_height(hbmp);
	PictureInfo->data_size = lzma_get_rowsize(hbmp) * PictureInfo->header.h;
	PictureInfo->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	
	if(Addr){
		PictureInfo->data = (void *)Addr;
	}else{
		data_tatol += PictureInfo->data_size;
		eLIBs_printf("PictureInfo->data_size=%d, data_tatol = %d\n", PictureInfo->data_size, data_tatol);
		PictureInfo->data = (void *)esMEMS_Balloc(PictureInfo->data_size);
	}

	if(PictureInfo->data == NULL){
		__wrn("ERR: wboot_malloc failed\n");
        goto error;
	}

	eLIBs_memset(PictureInfo->data, 0, PictureInfo->data_size);

	lzma_get_matrix(hbmp, PictureInfo->data);

	lzma_close_bmp(hbmp);
	hbmp = NULL;

    return 0;

error:
	lzma_close_bmp(hbmp);
	
	return -1;
}

#if 1
__s32 Lzma_get_theme(uint32_t theme_id, HTHEME	* bmp)
{
	HTHEME_i htheme = NULL;
    const U8 *pSrc;
    
	dsk_theme_init("d:\\apps\\theme.bin");
	if(*bmp == NULL)
	{
		*bmp = dsk_theme_open(theme_id);
		//eLIBs_printf("Lzma_get_theme: *bmp=0x%x\n", *bmp);
		return -1;
	}
	
	return 0;
}

__s32 Lzma_get_theme_buf(uint32_t theme_id, HTHEME	* bmp, lv_img_dsc_t *icon)
{
	HTHEME_i htheme = NULL;
    const U8 *pSrc;

    if(*bmp == NULL)
    {
		eLIBs_printf("Lzma_get_theme_buf(*bmp == NULL)\n");
    	return -1;
    }
    
	pSrc = (const U8 *)dsk_theme_hdl2buf(*bmp);
	htheme = (HTHEME_i)(*bmp);
	//eLIBs_printf("*bmp=0x%x, htheme->buf=0x%x, htheme->size = %d\n", *bmp, htheme->buf, htheme->size);
	Lzma_Parse_Pic_BMP_ByPath(htheme->buf, htheme->size, icon, htheme->buf, 1);
	
	return 0;
}

#else
void *Lzma_get_theme_buf(uint32_t theme_id, HTHEME	* bmp, lv_img_dsc_t *icon)
{
	HTHEME_i htheme = NULL;
    const U8 *pSrc;
    
	dsk_theme_init("d:\\apps\\theme.bin");
	if(*bmp == NULL)
	{
		*bmp = dsk_theme_open(theme_id);
	}
	
	pSrc = (const U8 *)dsk_theme_hdl2buf(*bmp);
	htheme = (HTHEME_i)(*bmp);
	Lzma_Parse_Pic_BMP_ByPath(htheme->buf, htheme->size, icon, htheme->buf, 1);

	return 0;
}
#endif

