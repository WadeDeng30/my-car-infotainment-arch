#include "picture.h"
#include <elibs_stdio.h>
#include <eLIBs_az100.h>

//��Ϲ����޸��ļ�ͷʵ��
//ʹ��·������ͼƬ,��Ҫ�ֶ��ͷ��ڴ�
//����ѹ��
void* img_openByFilename1(char*filename)
{
	ES_FILE *pic = NULL;
	__u32 real_length = 0,input_length;

	__u8* input_buf = NULL;
	__u8* output_buf = NULL;
	__u8  AZ100_header[8];

	lv_img_dsc_t *dsc;
	__s32 ret;
__u32 time;
	if(filename == NULL)
	{
		return NULL;
	}
	pic = eLIBs_fopen(filename,"rb");
	if(
pic == NULL)
	{
		eLIBs_printf("open pic err!\n");
		return NULL;
	}
	eLIBs_fseek(pic, 0, SEEK_END);
	input_length = eLIBs_ftell(pic);
	eLIBs_fseek(pic, 0, SEEK_SET);

	eLIBs_fread(AZ100_header,1,8,pic);//AZ100,length
	eLIBs_fseek(pic, 0, SEEK_SET);

	real_length = AZ100_GetUncompressSize(AZ100_header,input_length);
	if(real_length!=0)//ͼƬ��ѹ����
	{
		eLIBs_printf("pic is press! real_length = %d\n",real_length);
		input_buf  = (__u8*)esMEMS_Balloc(input_length);
		if(input_buf == NULL)
		{
			return NULL;
		}
		output_buf = (__u8*)esMEMS_Balloc(real_length);
		if(output_buf == NULL)
		{
			esMEMS_Bfree(input_buf,input_length);
			return NULL;
		}

		ret = eLIBs_fread(input_buf,1,input_length,pic);	//����ͼƬ���ڴ�
		eLIBs_printf("fread size = %d\n",ret);
		time = esKRNL_TimeGet();
		ret = AZ100_Uncompress(input_buf,input_length,output_buf,real_length);//��ѹͼƬ
		eLIBs_printf("unpress time = %d\n",esKRNL_TimeGet()-time);
		if(EPDK_FAIL == ret)
		{
			eLIBs_printf("***********AZ100_Uncompress fail***********\n");
			esMEMS_Bfree(input_buf,input_length);
			esMEMS_Bfree(output_buf,real_length);
			return NULL;
		}

		dsc = (lv_img_dsc_t*)output_buf;
		dsc->data = (__u8*)output_buf+sizeof(lv_img_dsc_t);//�޸�ͼƬ����
		esMEMS_Bfree(input_buf,input_length);
	}
	else//ͼƬû�б�ѹ��
	{
		input_buf  = (__u8*)esMEMS_Balloc(input_length);    //����ͼƬ�ڴ�
		if(input_buf == NULL)
		{
			return NULL;
		}
		ret = eLIBs_fread(input_buf,1,input_length,pic);	//����ͼƬ���ڴ�
		dsc = (lv_img_dsc_t*)input_buf;
		dsc->data = (__u8*)input_buf+sizeof(lv_img_dsc_t);  //�޸�ͼƬ����
	}
	eLIBs_fclose(pic);
	return dsc;
}






//��Ϲ����޸��ļ�ͷʵ��
//ʹ��·������ͼƬ,��Ҫ�ֶ��ͷ��ڴ�
//ͬһ��ͼƬ�����õ�����£��ڴ��л����������ͬ��ͼƬ���ᵼ����Դ�˷ѡ�
void* img_openByFilename(char*filename)
{
	ES_FILE *pic = NULL;
	__u32 total_length = 0;
	lv_img_dsc_t *dsc;
	if(filename == NULL)
	{
		return NULL;
	}
	pic = eLIBs_fopen(filename,"rb");
	if(
pic == NULL)
	{
		eLIBs_printf("open pic err!\n");
		return NULL;
	}
	eLIBs_fseek(pic, 0, SEEK_END);
	total_length = eLIBs_ftell(pic);
	eLIBs_fseek(pic, 0, SEEK_SET);

	dsc = (lv_img_dsc_t *)eLIBs_malloc(total_length);
	eLIBs_fread(dsc,1,total_length,pic);
	dsc->data = (__u8*)dsc+sizeof(lv_img_dsc_t);

	eLIBs_fclose(pic);
	return dsc;
}

__s32 img_closeByHandle(void*buf)
{
	if(buf)
	{
		eLIBs_free(buf);
	}
	return EPDK_OK;
}


//��Ϲ����޸��ļ�ͷʵ��
//ֱ��ʹ��id������Դ
//��Ҫ����theme.bin
lv_img_dsc_t* img_openById(__u32 res_id)
{
	HTHEME img_handle;
	lv_img_dsc_t *head=NULL;

	img_handle = dsk_theme_open(res_id);
	head = (lv_img_dsc_t *)dsk_theme_hdl2buf(img_handle);
	head->data = (__u8*)head+sizeof(lv_img_dsc_t);

	eLIBs_printf("img_info cf = %d w = %d h = %d size = %d\n",head->header.cf,head->header.w,head->header.h,head->data_size);

	return head;

}

static __u32 Read32(const __u8 ** ppData) {
	const __u8 * pData;
	__u32  Value;
	pData = *ppData;
	Value = *pData;
	Value |= (     *(pData + 1) << 8);
	Value |= ((__u32)*(pData + 2) << 16);
	Value |= ((__u32)*(pData + 3) << 24);
	pData += 4;
	*ppData = pData;
	return Value;
}

//ʹ��ԭʼ����ʵ��
//img_infoΪ��̬����ȫ�ֱ���
lv_img_dsc_t* img_open(img_info_t *img_info,int res_id)
{
	HTHEME img_handle;
	void * img_buffer = NULL;
	const __u8 * pread = NULL;
	__s32 width,height;

	img_handle = dsk_theme_open(res_id);
	img_buffer = dsk_theme_hdl2buf(img_handle);
	pread  = img_buffer;

	pread+=18;//read bmp head
	width  = Read32(&pread);
	height = Read32(&pread);
	pread  +=28;

	img_info->img_header.always_zero = 0;
	img_info->img_header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	img_info->img_header.h = (height<0)?(-height):(height);
	img_info->img_header.w = width;
	//__log("pic w = %d h = %d\n",img_info->img_header.w,img_info->img_header.h);
	img_info->img_dsc.data = pread;
	img_info->img_dsc.data_size = width*(img_info->img_header.h)*4;
	img_info->img_dsc.header = img_info->img_header;
	return &(img_info->img_dsc);
}


//ʹ��ԭʼ��������ͼƬ��
//ʹ��ȫ�ֻ�̬������������ͷ
lv_img_dsc_t img_open2(__u32 res_id)
{
	HTHEME img_handle;
	void * img_buffer = NULL;
	const __u8 * pread = NULL;
	__s32 width,height;
	lv_img_dsc_t img_info;

	img_handle = dsk_theme_open(res_id);
	img_buffer = dsk_theme_hdl2buf(img_handle);
	pread  = img_buffer;

	pread+=18;//read bmp head
	width  = Read32(&pread);
	height = Read32(&pread);
	pread  +=28;

	img_info.header.always_zero = 0;
	img_info.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	img_info.header.h = (height<0)?(-height):(height);
	img_info.header.w = width;
	//__log("pic w = %d h = %d\n",img_info->img_header.w,img_info->img_header.h);
	img_info.data = pread;
	img_info.data_size = width*(img_info.header.h)*4;
	return img_info;
	//return (img_info);
}


//ʹ�ñ������ر�ͼƬ
__s32 img_close(int res_id)
{
	dsk_theme_close(res_id);
}
