/*
************************************************************************************************************************
*                                                        BITMAP
*
*                             Copyright(C), 2006-2009, SoftWinners Microelectronic Co., Ltd.
*											       All Rights Reserved
*
* File Name : bmp.c
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
#ifndef  __lzma_bmp_c
#define  __lzma_bmp_c

//#include  "..\\Esh_shell_private.h"
#include  "lzma_bmp.h"

#if 0//def SKILL_FOR_SHUN_LISTCHANNEL_REPEATED //mllee 110824 
	#define __msg(...)    		(eLIBs_printf("MSG:L%d(%s):", __LINE__, __FILE__),                 \
							     eLIBs_printf(__VA_ARGS__))	
	#define __inf(...)    		(eLIBs_printf("MSG:L%d(%s):", __LINE__, __FILE__),                 \
							     eLIBs_printf(__VA_ARGS__))	
	//#define __here__            eLIBs_printf("@L%d(%s)\n", __LINE__, __FILE__);
#endif 

//#define LOGO_IN_MEM //mllee 131206

#define abs(x) (x) >= 0 ? (x):-(x)

#define PALETTE_OFF           ( sizeof(bmp_file_head_t_lzma) + sizeof(bmp_info_head_t_lzma) )
#define spcl_size_align( x, y )         ( ( (x) + (y) - 1 ) & ~( (y) - 1 ) )

static HRES         hres    = NULL;
static HRES         h_kres  = NULL;// add by hot lee for  karaoke
static __u32        style   = 0;

static char* g_pbmp_buf = NULL;
static __s32 g_bmp_size = 0;
static uint32_t theme_tatol = 0;

HBMP_lzma  lzma_open_bmp( __u8 *bmp_file, __u32 size,HBMP_i_t_lzma *hbmp_hdl_buf ,__u8 mem)
{
	//lv_fs_file_t *fp = NULL;
	void *fp = NULL;
	bmp_file_head_t_lzma  file_head;
	bmp_info_head_t_lzma  info_head;
	HBMP_i_lzma hdl = NULL;
	__u32  byte_count;
    char* pbuf=NULL;
    __u32 file_len;
    __u32 read_len;
    char* uncompress_buf=NULL;
    __u32 uncompress_len;

    hdl = NULL;

    pbuf = NULL;
    file_len = 0;
    uncompress_buf = NULL;
    uncompress_len = 0;

	if( bmp_file == NULL )
		return NULL;

    hdl = (HBMP_i_lzma)hbmp_hdl_buf;
	if( hdl == NULL )
		goto error;
		
	if(mem==1)
	{

		file_len = size;//sizeof(bmp_file);
		pbuf = bmp_file;
	}
	else
	{
		#if 0
		lv_fs_res_t res = lv_fs_open(fp, bmp_file, LV_FS_MODE_RD);
		if(res == LV_RES_OK)
		{
			__wrn("Error in open bmp file %s.\n", bmp_file);
			hdl = NULL;
			goto error;
		}
		lv_fs_seek(fp, 0, LV_FS_SEEK_END);
		lv_fs_tell(fp, &file_len);
		lv_mem_alloc(sizeof(bmp_dsc_t));
		#else
		__msg("bmp_file = %s\n", bmp_file);
		fp = eLIBs_fopen( bmp_file, "rb" );
		if( fp == NULL )
		{
			__wrn("Error in open bmp file %s.\n", bmp_file);
	        hdl = NULL;
			goto error;
		}

	    eLIBs_fseek(fp, 0, SEEK_END);
	    file_len = eLIBs_ftell(fp);
	    #endif
	    __msg("file_len=%d\n", file_len);
		pbuf = esMEMS_Palloc((file_len+1023)/1024, 0);
	    __msg("esMEMS_Palloc: pbuf=%x\n", pbuf);
	    if(!pbuf)
	    {        
	        __msg("palloc fail...\n");
	        hdl = NULL;
	        goto error;
	    }
	    eLIBs_fseek(fp, 0, SEEK_SET);
	    read_len = eLIBs_fread(pbuf, 1, file_len, fp);
	    if(read_len != file_len)
	    {
	        __msg("fread fail...\n");
	        hdl = NULL;
	        goto error;
	    }
	}
	
    //Î´Ñ¹ËõµÄ±³¾°
    if(EPDK_FALSE == AZ100_IsCompress(pbuf, file_len))
    {
        __msg("bg pic is uncompress...\n");
		if(mem == 1)
		{
			 uncompress_len = file_len;
			 uncompress_buf = esMEMS_Palloc((uncompress_len+1023)/1024, 0);
		        //__msg("esMEMS_Palloc: uncompress_buf=%x\n", uncompress_buf);
		        
		        if(!uncompress_buf)
		        {            
		            __msg("palloc fail...\n");
		            hdl = NULL;
		            goto error;
		        }
			 eLIBs_memcpy(uncompress_buf,pbuf,uncompress_len);
		}
		else
		{

		        uncompress_buf = pbuf;
		        uncompress_len = file_len;
		}
		
        pbuf = NULL;
        file_len = 0;
        __msg("uncompress_buf=%x\n", uncompress_buf);
    }
    else//´øÑ¹ËõµÄ±³¾°
    {
        __s32 ret_val;
        
        //__msg("bg pic is compress...\n");
        uncompress_len = AZ100_GetUncompressSize(pbuf, file_len);
        
        uncompress_buf = esMEMS_Palloc((uncompress_len+1023)/1024, 0);
        //__msg("esMEMS_Palloc: uncompress_buf=%x\n", uncompress_buf);
        
        if(!uncompress_buf)
        {            
            __msg("palloc fail...\n");
            hdl = NULL;
            goto error;
        }

        ret_val = AZ100_Uncompress(pbuf, file_len, uncompress_buf, uncompress_len);
        if(EPDK_FAIL == ret_val)
        {
            __msg("AZ100_Uncompress fail...\n");
            hdl = NULL;
            goto error;
        }
        //__here__;

		if(mem!=1)
		{
	        esMEMS_Pfree(pbuf, (file_len+1023)/1024);
	        __msg("esMEMS_Pfree: pbuf=%x, file_len=%d\n", pbuf, file_len);
		}
		
        pbuf = NULL;  
        file_len = 0;
        //__here__;
    }     

    if(0)
    {
        int i;

        __msg("begin dump uncompress_buf\n");

        for(i = 0 ; i < 500 && i < uncompress_len; i++)
        {
            eLIBs_printf("%2x ", uncompress_buf[i]);
            if((i%17) == 16)
            {
                eLIBs_printf("\n");
            }
        }
        __msg("\nend dump uncompress_buf\n");
    }

    g_pbmp_buf = uncompress_buf;
    g_bmp_size = uncompress_len;

    uncompress_buf = NULL;
    uncompress_len = 0;
    
	/* get bmp file head */	
    eLIBs_memcpy(&file_head, g_pbmp_buf, sizeof(file_head));
	if( file_head.bfType[0] != 'B' || file_head.bfType[1] != 'M' )
	{
        hdl = NULL;
		goto error;
	}
	
	/* get bmp information head */	
    eLIBs_memcpy(&info_head, (char*)g_pbmp_buf+sizeof(file_head), sizeof(info_head));
	if( info_head.biBitCount != 32 
        &&info_head.biBitCount != 16 
        &&info_head.biBitCount != 8 
        &&info_head.biBitCount != 4 
        &&info_head.biBitCount != 1)
	{
		__wrn("Error! bitcount is %d, not supported.\n", info_head.biBitCount);
        hdl = NULL;
		goto error;
	}
	
	hdl->width  = info_head.biWidth ;
	hdl->height = info_head.biHeight;
	hdl->bitcount = info_head.biBitCount;

	__msg("hdl->width=%d,hdl->height=%d,hdl->bitcount=%d\n",hdl->width,hdl->height,hdl->bitcount);

    //bit per line aligned by 8
	hdl->row_size = spcl_size_align( hdl->width * hdl->bitcount, 8);

    //byte per line
    hdl->row_size >>= 3;

    hdl->real_row_size = hdl->row_size;

    //byte per line aligned by 4
    hdl->row_size = spcl_size_align( hdl->row_size, 4);
    
	hdl->matrix_off = file_head.bfOffBits;

error:
//#ifndef LOGO_IN_MEM //mllee 131206
	if(mem!=1)
	{

		if( fp != NULL )
		{
			eLIBs_fclose( fp );
			fp = NULL;
		}
	    if(pbuf)
	    {
	        esMEMS_Pfree(pbuf, (file_len+1023)/1024);
	        pbuf = NULL;
	    }
	}
//#endif
    if(uncompress_buf)
    {
        esMEMS_Pfree(uncompress_buf, (uncompress_len+1023)/1024);
        uncompress_buf = NULL;      
    }
    
	return (HBMP_lzma)hdl;
}



__s32 lzma_close_bmp( HBMP_lzma hbmp )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;
	
	if( hdl == NULL )
		return -1;
		
	if(g_pbmp_buf)
    {
        __msg("esMEMS_Pfree: g_pbmp_buf=%x, g_bmp_size=%d\n", g_pbmp_buf, g_bmp_size);
        esMEMS_Pfree(g_pbmp_buf, (g_bmp_size+1023)/1024);
        g_pbmp_buf = NULL;
        g_bmp_size = 0;
    }
	
	eLIBs_memset( hdl, 0, sizeof(HBMP_i_t_lzma) );
	
	return 0;
}

__s32 lzma_read_pixel( HBMP_lzma hbmp, __u32 x, __u32 y, full_color_t_lzma *color_p, __s32 origin_pos )
{
    /*
	__u32  offset;
	__u32  byte_count;
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;
	__u32  abs_height = abs(hdl->height);
	
	if( hbmp == NULL )
		return -1;
		
	if( x >= hdl->width || y >= abs_height )
		return -1;
		
	byte_count = hdl->byte_count;
	if( hdl->height > 0 )
	{
		if( origin_pos == LZMA_ORIGIN_POS_UPPER_LEFT )	
			offset = hdl->matrix_off + ( abs_height - 1 - y )* hdl->row_size + byte_count * x;
		else if( origin_pos == LZMA_ORIGIN_POS_LOWER_LEFT )
			offset = hdl->matrix_off + y * hdl->row_size + byte_count * x;
		else
			return -1;  
	}
	else
	{
		if( origin_pos == LZMA_ORIGIN_POS_UPPER_LEFT )	
			offset = hdl->matrix_off + y * hdl->row_size + byte_count * x;
		else if( origin_pos == LZMA_ORIGIN_POS_LOWER_LEFT )
			offset = hdl->matrix_off + ( abs_height - 1 - y ) * hdl->row_size + byte_count * x;
		else
			return -1;  
	}

    if(g_pbmp_buf)
    {
	    eLIBs_memcpy(color_p, (char*)g_pbmp_buf+offset, byte_count);
    }
	*/
	return 0;
}

__s32  lzma_get_matrix( HBMP_lzma hbmp, void *buf  )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;
	
	__u32   row_size;
	__u32   real_row_size;
	__u32   height;
	__u32   i;
	__u8    *q;
	__s32   pad_count;
    __u32   bitCount;
	
	if( hbmp == NULL || buf == NULL || g_pbmp_buf == NULL)
		return -1;
		
	row_size = hdl->row_size;
    real_row_size = hdl->real_row_size;
	height = abs(hdl->height);
	pad_count = row_size - real_row_size;
    bitCount = hdl->bitcount;

    if( hdl->height < 0 )
	{
        __u8 *pSrc = (__u8 *)g_pbmp_buf+hdl->matrix_off;
        		
		q = (__u8 *)buf;
		for( i = 0;  i < height;  i++ )
		{
            eLIBs_memcpy(q, pSrc, real_row_size);
            pSrc += row_size;
			q += real_row_size;
		}
	}
	else
	{
        __u8 *pSrc = (__u8 *)g_pbmp_buf+hdl->matrix_off;
		
		q = (__u8 *)buf + real_row_size * ( height - 1 );
		for( i = 0;  i < height;  i++ )
		{
            eLIBs_memcpy(q, pSrc, real_row_size);
            pSrc += row_size;
			q -= real_row_size;
		}
	}    
    
	return 0;

}

__u32   lzma_get_bitcount( HBMP_lzma hbmp )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;

	return ( hdl->bitcount );
}	
	
__u32   lzma_get_width( HBMP_lzma hbmp )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;

	return ( hdl->width );
}	

__s32  lzma_get_height( HBMP_lzma hbmp )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;

	return abs( hdl->height );
}	

__u32   lzma_get_rowsize( HBMP_lzma hbmp )
{
	HBMP_i_lzma hdl = (HBMP_i_lzma)hbmp;

	return ( hdl->row_size );
}

#if 0
int32_t lzma_dsk_theme_init(const char *file)
{
    if (hres == NULL)
    {
        hres = OpenRes((char *)file, 0);

        if (hres == NULL)
        {
            __err("Error in opening resource file \"%s\".", file);
            return EPDK_FAIL;
        }
    }

    return EPDK_OK;
}

static void  _release_theme_res(HTHEME_i htheme)
{
    if (htheme != NULL)
    {
        if (htheme->buf != NULL)
        {
            esMEMS_Bfree(htheme->buf, htheme->size);
            htheme->buf = NULL;
        }

        esMEMS_Mfree(0, htheme);
    }
}

HTHEME lzma_dsk_theme_open(uint32_t theme_id)
{
    int32_t     ret;
    HTHEME_i    htheme = NULL;
    void        *pbuf;

    if (hres == NULL)
    {
        return 0;
    }

    htheme  = (HTHEME_i) esMEMS_Malloc(0, sizeof(theme_t));

    if (htheme == NULL)
    {
        __wrn("Error in allocating memory.");
        return 0;
    }

    eLIBs_memset(htheme, 0, sizeof(theme_t));
    htheme->id          = theme_id;
    htheme->size_com    = GetResSize(hres, style, theme_id);
    pbuf                = esMEMS_Balloc(htheme->size_com);

    if (NULL == pbuf)
    {
        __wrn("*************mem not enough, theme_id=%d***********", theme_id);
        goto error;
    }

    ret     = GetRes(hres, 0, theme_id, pbuf, htheme->size_com);

    //__msg("hres=%x,theme_id=%d,pbuf=%x,htheme->size_com=%d", hres,theme_id, pbuf, htheme->size_com);
    if (-1 == ret)
    {
        __wrn("***********GetRes fail***********");
        goto error;
    }

    if (EPDK_TRUE == AZ100_IsCompress(pbuf, htheme->size_com))
    {
        htheme->size = AZ100_GetUncompressSize(pbuf, htheme->size_com);
    }
    else//Î´Ñ¹Ëõ
    {
        htheme->size = htheme->size_com;
    }

    if (htheme->size == 0)
    {
        __wrn("Fail in getting the size of res %u.", htheme->id);
        goto error;
    }

    if (pbuf)
    {
        esMEMS_Bfree(pbuf, htheme->size_com);
        pbuf = NULL;
    }

    return (HTHEME) htheme;
error:
    _release_theme_res(htheme);

    if (pbuf)
    {
        esMEMS_Bfree(pbuf, htheme->size_com);
        pbuf = NULL;
    }

    return 0;
}

void *lzma_dsk_theme_hdl2buf(HTHEME handle)
{
    __s32       size;
    __s32       ret;
    void        *pbuf;
    HTHEME_i    htheme = (HTHEME_i) handle;

    if (handle == 0)
    {
        __wrn("handle is null, handle to buf failed!");
        return NULL;
    }

    if (htheme->buf != NULL)
    {
        return htheme->buf;
    }
    
	theme_tatol += htheme->size;
	eLIBs_printf("htheme->size=%d, theme_tatol = %d\n", htheme->size, theme_tatol);

    htheme->buf = (void *)esMEMS_Balloc(htheme->size);

    if (htheme->buf == NULL)
    {
        __wrn("Error in alloc size %x.", htheme->size);
        return NULL;
    }

    pbuf = esMEMS_Balloc(htheme->size_com);

    if (NULL == pbuf)
    {
        __wrn("*************mem not enough***********");
        esMEMS_Bfree(htheme->buf, htheme->size);

        htheme->buf = NULL;
        return NULL;
    }

    ret = GetRes(hres, style, htheme->id, pbuf, htheme->size_com);

    if (-1 == ret)
    {
        __wrn("***********GetRes fail***********");
        esMEMS_Bfree(htheme->buf, htheme->size);
        esMEMS_Bfree(pbuf, htheme->size_com);

        htheme->buf = NULL;
        pbuf        = NULL;
        return NULL;
    }

    if (EPDK_TRUE == AZ100_IsCompress(pbuf, htheme->size_com))
    {
        __s32 ret;

        ret = AZ100_Uncompress(pbuf, htheme->size_com, htheme->buf, htheme->size);

        if (EPDK_FAIL == ret)
        {
            __wrn("***********AZ100_Uncompress fail***********");
            esMEMS_Bfree(htheme->buf, htheme->size);
            esMEMS_Bfree(pbuf, htheme->size_com);

            htheme->buf = NULL;
            pbuf        = NULL;
            return NULL;
        }
    }
    else
    {
        eLIBs_memcpy(htheme->buf, pbuf, htheme->size_com);
    }

    esMEMS_Bfree(pbuf, htheme->size_com);

    pbuf = NULL;
    return htheme->buf;
}
#endif
#endif     //  ifndef __lzma_bmp_c

/* end of lzma_bmp.c  */
