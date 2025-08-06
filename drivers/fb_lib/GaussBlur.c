/// <summary>  
/// 程序功能：c语言实现纯高斯模糊
/// 系统Ubuntu 15.10，GCC开发环境,编程语言C,最新整理时间 whd 2016.9.2。  
/// 参考代码：https://github.com/Duwaz/Gaussian_Blur
/// <remarks> 1: 能处理24位BMP格式图像。</remarks>    
/// <remarks> 2: 主程序无参数,默认处理工程目录下的input.bmp,处理后的结果为output.bmp。</remarks>    
/// <remarks> 3: 处理效果由高斯模糊半径决定:GaussBlur(bmp_t*, double)函数的第二个参数</remarks> 
/// </summary> 
#include<stdio.h>  
#include<stdlib.h>
#include <math.h>
#include "apps.h"
#include "GaussBlur.h"
#include "bg_bmp.h"



#if 0
	#define __msg(...)    		(eLIBs_printf("L%d(%s):", __LINE__, __FILE__),                 \
							     eLIBs_printf(__VA_ARGS__))	
 	#define __here__            eLIBs_printf("@L%d(%s)\n", __LINE__, __FILE__);
	#define __wrn(...) 			(eLIBs_printf("WRN:L%d(%s):", __LINE__, __FILE__), eLIBs_printf(__VA_ARGS__))
#endif

#define __here__

#if 0
#define ABS(x)                          ( (x) >0 ? (x) : -(x) )



typedef struct
{
	BITMAPINFOHEADER header;  //信息头
	char *data;//图像数据
} bmp_t;


typedef struct
{
	bmp_file_head_t file_head;  //文件头
	bmp_info_head_t info_head;  //信息头
	char *data;//图像数据
} bmp_t_ex;


int win_size(double sigma);  
double Gauss(double sigma, double x);
void gauss_bmp_free(bmp_t *bmp);
bmp_t* gauss_bmp_open(ES_FILE *f);
int gauss_bmp_write(bmp_t *bmp, ES_FILE *out);
bmp_t* GaussBlur(bmp_t *src, double sigma);
bmp_t_ex *GaussBlur_ex(HBMP  hbmp, double sigma); 
int gauss_bmp_write_ex(bmp_t_ex *bmp, ES_FILE *out);


/*
int GaussBlur_test(void)  
{  
    char *InputName, *OutputName;  //输入输出图像文件名变量
	ES_FILE *InputFile, *OutputFile;  //输入输出图像文件
	bmp_t *bmp = NULL, *blur = NULL;
    InputName = "f:\\input.bmp"; OutputName = "f:\\output.bmp";
	
    if(!(InputFile=fopen(InputName, "r")))  //图像文件打开函数,打不开返回,提示找不到文件
	{
		__msg("File not found\n");
		return 1;
	}
    bmp = gauss_bmp_open(InputFile);   //图像文件读取操作,前面必须有打开的操作才能读取文件数据
	fclose(InputFile);//关闭文件
    OutputFile = fopen(OutputName, "wb"); //打开并创建输出文件
    __here__
	if (OutputFile == NULL) 
	{
		__msg("Can't open %s\n", OutputName);
		return 1;
	}
    __here__
    blur = GaussBlur(bmp, 4.0); //模糊处理并返回处理后数据
    gauss_bmp_write(blur, OutputFile);//将处理后数据存放到新建的文件中 
    fclose(OutputFile); //关闭文件
    gauss_bmp_free(bmp);//释放存放读取数据的内存
    free(blur);//释放处理后图像数据的内存
    return 0;
}  
*/



int GaussBlur_test(void)  
{  
    char *InputName, *OutputName;  //输入输出图像文件名变量
	ES_FILE *InputFile, *OutputFile;  //输入输出图像文件
	bmp_t_ex *bmp = NULL, *blur = NULL;
    HBMP  hbmp = NULL;
    HBMP_i_t hbmp_buf;
  //  InputName = "c:\\boot_ui\\screen_scroll_bg1.bmp"; OutputName = "f:\\output.bmp";
	
    InputName = "f:\\input.bmp"; OutputName = "f:\\output.bmp";
	
    hbmp = open_bmp(InputName, &hbmp_buf);

    OutputFile = fopen(OutputName, "wb"); //打开并创建输出文件
    __here__
	if (OutputFile == NULL) 
	{
		__msg("Can't open %s\n", OutputName);
		return 1;
	}
    __here__
    blur = GaussBlur_ex(hbmp, 4.0); //模糊处理并返回处理后数据
	__msg("Width: %ld\n",blur->info_head.biWidth);
 	__msg("Height: %ld\n", blur->info_head.biHeight);
 	__msg("BitCount: %d\n\n", (int)blur->info_head.biBitCount);
 	__msg("SizeImage: %d\n\n", (int)blur->info_head.biSizeImage);
 	__msg("OffBits: %d\n\n", (int)blur->file_head.bfOffBits);

	
    gauss_bmp_write_ex(blur, OutputFile);//将处理后数据存放到新建的文件中 
    fclose(OutputFile); //关闭文件
    close_bmp(hbmp);//释放存放读取数据的内存
    free(blur);//释放处理后图像数据的内存
    
    return 0;
}  





/// <summary>  
/// 函数功能:给定一个BMP图像文件,将其中的数据读取出来,并返回图像数据  
/// 函数返回:图像的数据(信息头和数据)
/// 知识点:如何读取一个图像文件的数据 
/// </summary>  
/// <param name="f">图像文件存放位置地址。</param>  
bmp_t *gauss_bmp_open(ES_FILE *f) 
{
	bmp_t *bmp;
	char* pbuf;
    __u32 file_len;
    __u32 read_len;
	
	bmp = (bmp_t *)malloc(sizeof(bmp_t));
	bmp->data = NULL;


	if( f == NULL )
		return NULL;

    eLIBs_fseek(f, 0, SEEK_END);
    file_len = eLIBs_ftell(f);
    __msg("file_len=%d\n", file_len);

	pbuf = esMEMS_Palloc((file_len+1023)/1024, 0);
    __msg("esMEMS_Palloc: pbuf=%x\n", pbuf);
    if(!pbuf)
    {        
        __msg("palloc fail...\n");
		return NULL;
    }
    eLIBs_fseek(f, 0, SEEK_SET);
    read_len = eLIBs_fread(pbuf, 1, file_len, f);
    if(read_len != file_len)
    {
        __msg("fread fail...\n");
		return NULL;
    }

	memcpy(&(bmp->header),pbuf ,sizeof(BITMAPINFOHEADER));
	bmp->data = (char*)esMEMS_Palloc((bmp->header.biSizeImage+1023)/1024, 0);
	memcpy(bmp->data,pbuf+sizeof(BITMAPINFOHEADER),(read_len-sizeof(BITMAPINFOHEADER)));

	 __msg("图像读取成功\n");
     __msg("Width: %ld\n",bmp->header.biWidth);
     __msg("Height: %ld\n", bmp->header.biHeight);
     __msg("BitCount: %d\n\n", (int)bmp->header.biBitCount);

	
 
	/*if (fread(&(bmp->header), sizeof(BITMAPINFOHEADER), 1, f)) {
		bmp->data = (char*)malloc(bmp->header.biSizeImage);
		if (fread(bmp->data, bmp->header.biSizeImage, 1, f))
                 __msg("图像读取成功\n");
                 __msg("Width: %ld\n",bmp->header.biWidth);
                 __msg("Height: %ld\n", bmp->header.biHeight);
                 __msg("BitCount: %d\n\n", (int)bmp->header.biBitCount);
			return bmp;
	}*/
	__msg("Error reading file\n");
	gauss_bmp_free(bmp);
	return NULL;
}
/// <summary>  
/// 函数功能:释放存放图像数据的内存  
/// 函数返回:无
/// 知识点:释放图像数据内存 
/// </summary>  
/// <param name="bmp">图像数据变量。</param>  
void gauss_bmp_free(bmp_t *bmp)
{
	if (bmp == NULL) return;
	if (bmp->data != NULL) free(bmp->data);
	free(bmp);
}


/// <summary>  
/// 函数功能:将图像数据写入到图像文件中  
/// 函数返回:写入数据的个数
/// 知识点:将数据写入文件中(将数值从内存写入文件中)
/// </summary>  
/// <param name="bmp">图像数据变量。</param>  
/// <param name="out">文件流。</param>  
int gauss_bmp_write(bmp_t *bmp, ES_FILE *out) {
	return fwrite(&(bmp->header), sizeof(BITMAPINFOHEADER), 1, out)
		&& fwrite(bmp->data, bmp->header.biSizeImage, 1, out);
}

int gauss_bmp_write_ex(bmp_t_ex *bmp, ES_FILE *out) {
	int len=0;

	len += fwrite(&(bmp->file_head),1, sizeof(bmp_file_head_t), out);
	__msg("len=%d..\n",len);
	len += fwrite(&(bmp->info_head),1, sizeof(bmp_info_head_t), out); 
	__msg("len=%d..\n",len);
	__msg("bmp->info_head.biSizeImage=%d..\n",bmp->info_head.biSizeImage);
	//len += fwrite(bmp->data, 1,bmp->info_head.biSizeImage, out); 
	len += fwrite(bmp->data, bmp->info_head.biSizeImage,1, out); 
	__msg("len=%d..\n",len);

	
	return len;
	 
}

/// <summary>  
/// 函数功能:模糊窗的大小(根据高斯半径和模糊半径满足3sigma原则) 
/// 函数返回:模糊窗的大小,即长或宽(长=宽)
/// 知识点:高斯模糊的3*sigma原则
/// </summary>  
/// <param name="sigma">高斯核函数的参数sigma</param>  
int win_size(double sigma)
{
	return (1 + (((int)ceil(4 * sigma)) * 2)); 
}
/// <summary>  
/// 函数功能:单像素点计算高斯系数 
/// 函数返回:高斯系数
/// 知识点:高斯系数计算公式
/// </summary>  
/// <param name="sigma">高斯核函数的参数sigma</param>  
/// <param name="x">当前像素距离模糊窗中心的距离</param>  
double Gauss(double sigma, double x)
{
	return exp(-(x * x) / (2.0 * sigma * sigma)) / (sigma * SQRT_2PI);
}


/// <summary>  
/// 函数功能:计算高斯模糊窗下的每个像素对应的权值,计算一半就够了,因为是权值是对称的(一维高斯)
/// 函数返回:模糊窗下每个像素的权值
/// 知识点:模糊窗下的各点的高斯系数(一维数组)
/// </summary>  
/// <param name="sigma">高斯核函数的参数sigma</param>  
/// <param name="win_size">模糊窗的大小</param>  
double* GaussAlgorithm(int win_size, double sigma)
{
	int wincenter, x;
	double *kern, sum = 0.0;
	wincenter = win_size / 2;
	kern = (double*)calloc(win_size, sizeof(double));
    __here__
 
	for (x = 0; x < wincenter + 1; x++)
	{
		kern[wincenter - x] = kern[wincenter + x] = Gauss(sigma, x);
		sum += kern[wincenter - x] + ((x != 0) ? kern[wincenter + x] : 0.0);
	}
	for (x = 0; x < win_size; x++)
		kern[x] /= sum;
    __here__
 
	return kern;
}


/// <summary>  
/// 函数功能:高斯模糊实现函数
/// 函数返回:模糊后的图像
/// 知识点:模糊窗下的各点的高斯系数(数组)
/// </summary>  
/// <param name="src">待模糊的图像</param>  
/// <param name="sigma">模糊半径(高斯核函数参数)</param>  
/// <remarks> rgb三通道分别处理</remarks> 
bmp_t *GaussBlur(bmp_t *src, double sigma) 
{
	int	row, col, col_r, col_g, col_b, winsize, halfsize, k, count, rows, count1, count2, count3;
	int width, height;
	double  row_g, row_b, row_r, col_all;
	unsigned char  r_r, r_b, r_g, c_all;
	char *tmp;
	double *algorithm;
	bmp_t *blur;
 
	count=0;
	width = 3*src->header.biWidth; height = src->header.biHeight;
    __here__
	__msg("width=%d\n",width);
	__msg("height=%d\n",height);
 
	if ((width % 4) != 0) width += (4 - (width % 4)); 
	blur = (bmp_t*)malloc(sizeof(bmp_t));
	blur->header = src->header;
	blur->header.biWidth = src->header.biWidth;
	blur->header.biHeight = src->header.biHeight;
	blur->header.biSizeImage = width * blur->header.biHeight;
	blur->data = (char*)malloc(blur->header.biSizeImage);
    __here__
 
	winsize = win_size(sigma);
	algorithm = GaussAlgorithm(winsize, sigma); 
	winsize *= 3; 
	halfsize = winsize / 2;
	
	__msg("winsize=%d\n",winsize);
	__msg("algorithm=%d\n",algorithm);
    __here__
 
	__msg("width * height=%d\n",width * height);
	tmp = (char*)calloc(width * height, sizeof(char)); 
    __here__
	
	for (row = 0; row < height; row++)
	{
		col_r = 0;
		col_g = 1;
		col_b = 2;
		for (rows = 0; rows < width; rows += 3)
		{
			row_r = row_g = row_b = 0.0;
			count1 = count2 = count3 = 0;
		
			for (k = 1; k < winsize; k += 3)
			{
				if ((k + col_r - halfsize >= 0) && (k + col_r - halfsize < width))
				{
					r_r = *(src->data + row * width + col_r + k - halfsize);
					row_r += (int)(r_r)* algorithm[count1];
					count1++;
				}
				if ((k + col_g - halfsize >= 0) && (k + col_g - halfsize < width))
				{
					r_g = *(src->data + row * width + col_g + k - halfsize);
					row_g += (int)(r_g)* algorithm[count2];
					count2++;
				}
 
				if ((k + col_b - halfsize >= 0) && (k + col_b - halfsize < width))
				{
					r_b = *(src->data + row * width + col_b + k - halfsize);
					row_b += (int)(r_b)* algorithm[count3];
					count3++;
				}
			}
 
			*(tmp + row * width + col_r) = (unsigned char)(ceil(row_r));
			*(tmp + row * width + col_g) = (unsigned char)(ceil(row_g));
			*(tmp + row * width + col_b) = (unsigned char)(ceil(row_b));
			col_r += 3;
			col_g += 3;
			col_b += 3;
		}
	}
	
    __here__
	winsize /= 3;
	halfsize = winsize / 2;
	for (col = 0; col < width; col++)
		for (row = 0; row < height; row++)
		{
		col_all = 0.0;
		for (k = 0; k < winsize; k++)
			if ((k + row - halfsize >= 0) && (k + row - halfsize < height))
			{
			c_all = *(tmp + (row + k - halfsize) * width + col);
			col_all += ((int)c_all) * algorithm[k];
			}
		*(blur->data + row * width + col) = (unsigned char)(ceil(col_all));
		}
	
	free(tmp);
	free(algorithm);
    __here__
 
	return blur;
}


bmp_t_ex *GaussBlur_ex(HBMP  hbmp, double sigma) 
{
	int	row, col, col_a,col_r, col_g, col_b, winsize, halfsize, k, count, rows, count1, count2, count3,count4;
	int width, height;
	double  row_g, row_b, row_r,row_a, col_all;
	unsigned char  r_a,r_r, r_b, r_g, c_all;
	char *tmp;
	double *algorithm;
	bmp_t_ex *blur;
	char *Bdata=NULL;
 
	count=0;

	width = 4*get_width(hbmp); 
	height = get_height(hbmp);

	
	blur = (bmp_t_ex *)malloc(sizeof(bmp_t_ex));
	
    __here__
	__msg("width=%d\n",width);
	__msg("height=%d\n",height);
	

	get_file_head(hbmp,&blur->file_head);
	get_info_head(hbmp,&blur->info_head);
	__msg("blur->file_head.bfType[0]=%c\n",blur->file_head.bfType[0]);
	__msg("blur->file_head.bfType[1]=%c\n",blur->file_head.bfType[1]);
	__msg("blur->file_head.bfSize=%d\n",blur->file_head.bfSize);
	__msg("blur->file_head.bfReserved=%d\n",blur->file_head.bfReserved);
	__msg("blur->file_head.bfOffBits=%d\n",blur->file_head.bfOffBits);


	__msg("blur->info_head.biWidth=%d\n",blur->info_head.biWidth);
	__msg("blur->info_head.biHeight=%d\n",blur->info_head.biHeight);
	__msg("blur->info_head.biBitCount=%d\n",blur->info_head.biBitCount);
	__msg("blur->info_head.biSizeImage=%d\n",blur->info_head.biSizeImage);

	Bdata = (char*)esMEMS_Palloc((blur->info_head.biSizeImage+1023)/1024, 0);

	if(!Bdata)
	{
		__msg("Bdata is null \n");
		return ;
	}
	get_data(hbmp,Bdata);
	//get_matrix(hbmp,Bdata);
    __here__
 
	if ((width % 4) != 0) width += (4 - (width % 4)); 
	
	
	//blur->header = src->header;
	//blur->header.biWidth = src->header.biWidth;
	//blur->header.biHeight = src->header.biHeight;
	blur->info_head.biSizeImage = width * height;
	
	blur->data = (char*)esMEMS_Palloc((blur->info_head.biSizeImage+1023)/1024, 0);
    __here__
 
	winsize = win_size(sigma);
	algorithm = GaussAlgorithm(winsize, sigma); 
	winsize *= 4; 
	halfsize = winsize / 2;
	
	__msg("winsize=%d\n",winsize);
	__msg("algorithm=%d\n",algorithm);
    __here__
 
	__msg("width * height=%d\n",width * height);
	tmp = (char*)calloc(width * height, sizeof(char)); 
    __here__
	
	for (row = 0; row < height; row++)
	{
		col_a = 0;
		col_r = 1;
		col_g = 2;
		col_b = 3;
		for (rows = 0; rows < width; rows += 4)
		{
			row_a = row_r = row_g = row_b = 0.0;
			count1 = count2 = count3 = count4 = 0;
		
			for (k = 0; k < winsize; k += 3)
			{
				if ((k + col_a - halfsize >= 0) && (k + col_a - halfsize < width))
				{
					r_a = *(Bdata + row * width + col_a + k - halfsize);
					row_a += (int)(r_a)* algorithm[count1];
					count1++;
				}
			
				if ((k + col_r - halfsize >= 0) && (k + col_r - halfsize < width))
				{
					r_r = *(Bdata + row * width + col_r + k - halfsize);
					row_r += (int)(r_r)* algorithm[count2];
					count2++;
				}
				if ((k + col_g - halfsize >= 0) && (k + col_g - halfsize < width))
				{
					r_g = *(Bdata + row * width + col_g + k - halfsize);
					row_g += (int)(r_g)* algorithm[count3];
					count3++;
				}
 
				if ((k + col_b - halfsize >= 0) && (k + col_b - halfsize < width))
				{
					r_b = *(Bdata + row * width + col_b + k - halfsize);
					row_b += (int)(r_b)* algorithm[count4];
					count4++;
				}
			}
 
 			*(tmp + row * width + col_a) = (unsigned char)(ceil(row_a));
			*(tmp + row * width + col_r) = (unsigned char)(ceil(row_r));
			*(tmp + row * width + col_g) = (unsigned char)(ceil(row_g));
			*(tmp + row * width + col_b) = (unsigned char)(ceil(row_b));
			col_a += 4;
			col_r += 4;
			col_g += 4;
			col_b += 4;
		}
	}
	
    __here__
	winsize /= 4;
	halfsize = winsize / 2;
	for (col = 0; col < width; col++)
		for (row = 0; row < height; row++)
		{
			col_all = 0.0;
			for (k = 0; k < winsize; k++)
				if ((k + row - halfsize >= 0) && (k + row - halfsize < height))
				{
					c_all = *(tmp + (row + k - halfsize) * width + col);
					col_all += ((int)c_all) * algorithm[k];
				}
			*(blur->data + row * width + col) = (unsigned char)(ceil(col_all));
		}
	
	free(tmp);
	free(algorithm);
    __here__
 
	return blur;
}
#else

iir_coefs_t iir_coefs;

typedef struct
{
	bmp_file_head_t file_head;  //文件头
	bmp_info_head_t info_head;  //信息头
	char *data;//图像数据
} bmp_t_ex;



__s32 add(__s32 x, __s32 y)
{ 
   return (x+y);
}
__s32 sub(__s32 x, __s32 y)
{
	return (x-y);
}      

__s32 mul(__s32 x,__s32 y)
{
	return ((x*y)>>Qx);
}   

__s32 _div(__s32 x,__s32 y)
{
   return ((x<<Qx)/y);
}   

__s32 float_to_fixed(float x)
{
   int i;
   __s32 n;
  
   for(i=0;i<Qx;i++) 
   {
      x *= 2;
   }
   n=(__s32)x;
   return n;
}

float fixed_to_float(__s32 x)
{
    int i;
    float n;
    n = x;
    for(i=0;i<Qx;i++) 
    {
        n/=2;
    }  
    return n;
}

void __cvti32_convert(__s32 *pData, /*unsigned */int *x)
{
	*(pData+3) = /*(__s32)*/(((*x)&0xff000000) >> 16);
	*(pData+2) = /*(__s32)*/(((*x)&0x00ff0000) >> 8);
	*(pData+1) = /*(__s32)*/(((*x)&0x0000ff00));
	*pData = 	/*(__s32)*/((*x)&0x000000ff)<<8;
	
	return;
}

void CaculateCoefs(float sigma, iir_coefs_t *coefs)
{
	__s32 alpha, lamma, k, tmp; 
	float f_alpha,f_lamma,f_tmp;
	//sigma = 3.0;
	if(sigma < 0.5) //不小于0.5; 0.5*2^Q_VALUE
		sigma = 0.5;

	f_alpha = (float) expf((0.726)*(0.726)) /sigma;
	f_lamma = (float)expf(-f_alpha);
	f_tmp = (float)expf(-2*f_alpha);
	coefs->b2 = float_to_fixed(f_tmp);
	alpha = float_to_fixed(f_alpha);
	lamma = float_to_fixed(f_lamma);
	//k
	tmp = sub(0x100,lamma);
	tmp = mul(tmp,tmp);
	k = tmp;
	tmp = mul(mul(0x200,alpha),lamma);
	tmp = sub(add(0x100,tmp),coefs->b2);
	k = _div(k,tmp);
	coefs->a0 = k;
	coefs->a1 = mul(mul(k,sub(alpha,0x100)),lamma);
	coefs->a2 = mul(mul(k,add(alpha,0x100)),lamma);
	coefs->a3 = mul(-k,coefs->b2);
	coefs->b1 = mul(-512,lamma);
	coefs->cprev = add(coefs->a0,coefs->a1);
	tmp = add(add(0x100,coefs->b1),coefs->b2);
	coefs->cprev = _div(coefs->cprev,tmp);
	coefs->cnext = add(coefs->a2,coefs->a3);
	coefs->cnext = _div(coefs->cnext,tmp);
	
	//__msg("sigma= %x, %f, %d\n",sigma,sigma,(int)sigma);
	//__msg("a0 = %.3f, a1 = %.3f, a2 = %.3f,a3 = %.3f\n",coefs->a0,coefs->a1,coefs->a2,coefs->a3);
	return;
}

//水平方向像素模糊;
void GaussHorizontal(iir_coefs_t *coefs, int width, int height,__s32 *temp,  /*unsigned */int* in, __s32 *out)
{
	__s32 prevIn[4], currIn[4], prevOut[4], prev2Out[4], /*currComp[4],*/temp1[4],/*temp2[4],temp3[4],*/coeft;
	int j,y;
	__s32 coefa0,coefa1,inNext[4],output[4];

	//边界处理;
	coeft = coefs->cprev;
	__cvti32_convert(&prevIn[0], in);
	prev2Out[0] = mul(prevIn[0],coeft);
	prevOut[0] = prev2Out[0];
	prev2Out[1] = mul(prevIn[1],coeft);
	prevOut[1] = prev2Out[1];
	prev2Out[2] = mul(prevIn[2],coeft);
	prevOut[2] = prev2Out[2];
	prev2Out[3] = mul(prevIn[3],coeft);
	prevOut[3] = prev2Out[3];

	//left-to-right pass
	y = width;
	for (; y > 0; y--) 
	{
		__cvti32_convert(&currIn[0],in);
		
		temp1[0] = prevOut[0];
		prevOut[0] = sub(add(mul(currIn[0],coefs->a0),mul(prevIn[0],coefs->a1)),add(mul(prevOut[0],coefs->b1),mul(prev2Out[0],coefs->b2)));
		prev2Out[0] = temp1[0];
		prevIn[0] = currIn[0];
		*(temp+0) = prevOut[0];
		
		temp1[0] = prevOut[1];
		prevOut[1] = sub(add(mul(currIn[1],coefs->a0),mul(prevIn[1],coefs->a1)),add(mul(prevOut[1],coefs->b1),mul(prev2Out[1],coefs->b2)));
		prev2Out[1] = temp1[0];
		prevIn[1] = currIn[1];
		*(temp+1) = prevOut[1];

		temp1[0] = prevOut[2];
		prevOut[2] = sub(add(mul(currIn[2],coefs->a0),mul(prevIn[2],coefs->a1)),add(mul(prevOut[2],coefs->b1),mul(prev2Out[2],coefs->b2)));
		prev2Out[2] = temp1[0];
		prevIn[2] = currIn[2];
		*(temp+2) = prevOut[2];

		temp1[0] = prevOut[3];
		prevOut[3] = sub(add(mul(currIn[3],coefs->a0),mul(prevIn[3],coefs->a1)),add(mul(prevOut[3],coefs->b1),mul(prev2Out[3],coefs->b2)));
		prev2Out[3] = temp1[0];
		prevIn[3] = currIn[3];
		*(temp+3) = prevOut[3];

		in += 1 ; temp += 4;
	}

	// right-to-left horizontal pass
	in -= 1;
	temp -=4;

	out += 4*(width-1);

	//边界处理;
	coeft = coefs->cnext;
	__cvti32_convert(&prevIn[0],in);
	
	prev2Out[0] = mul(prevIn[0],coeft);
	prevOut[0] = prev2Out[0];
	currIn[0] = prevIn[0];
	
	prev2Out[1] = mul(prevIn[1],coeft);
	prevOut[1] = prev2Out[1];
	currIn[1] = prevIn[1];

	prev2Out[2] = mul(prevIn[2],coeft);
	prevOut[2] = prev2Out[2];
	currIn[2] = prevIn[2];

	prev2Out[3] = mul(prevIn[3],coeft);
	prevOut[3] = prev2Out[3];
	currIn[3] = prevIn[3];

	////
	coefa0 = coefs->a2;
	coefa1 = coefs->a3;

	for (y = width-1; y >= 0; y--) 
	{
		__cvti32_convert(&inNext[0],in);
		
		temp1[0] = prevOut[0];
		prevOut[0] = sub(add(mul(currIn[0],coefa0),mul(prevIn[0],coefa1)),add(mul(prevOut[0],coefs->b1),mul(prev2Out[0],coefs->b2)));
		*(out+0) = add(*(temp+0),prevOut[0]); 
		prev2Out[0] = temp1[0];
		prevIn[0] = currIn[0];
		currIn[0] = inNext[0];	
		
		temp1[0] = prevOut[1];
		prevOut[1] = sub(add(mul(currIn[1],coefa0),mul(prevIn[1],coefa1)),add(mul(prevOut[1],coefs->b1),mul(prev2Out[1],coefs->b2)));
		*(out+1) = add(*(temp+1),prevOut[1]); 
		prev2Out[1] = temp1[0];
		prevIn[1] = currIn[1];
		currIn[1] = inNext[1];	
		
		temp1[0] = prevOut[2];
		prevOut[2] = sub(add(mul(currIn[2],coefa0),mul(prevIn[2],coefa1)),add(mul(prevOut[2],coefs->b1),mul(prev2Out[2],coefs->b2)));
		*(out+2) = add(*(temp+2),prevOut[2]); 
		prev2Out[2] = temp1[0];
		prevIn[2] = currIn[2];
		currIn[2] = inNext[2];	
		
		temp1[0] = prevOut[3];
		prevOut[3] = sub(add(mul(currIn[3],coefa0),mul(prevIn[3],coefa1)),add(mul(prevOut[3],coefs->b1),mul(prev2Out[3],coefs->b2)));
		*(out+3) = add(*(temp+3),prevOut[3]); 
		prev2Out[3] = temp1[0];
		prevIn[3] = currIn[3];
		currIn[3] = inNext[3];	

		in -= 1; out -= 4; 
		temp -=4;
	}
}

//垂直方向像素模糊;
void GaussVertical(iir_coefs_t *coefs, int width, int height,__s32 *temp,  __s32* in, /*unsigned*/ int *out)
{
	__s32 prevIn[4], currIn[4], prevOut[4], prev2Out[4], /*currComp[4],*/temp1[4],/*temp2[4],temp3[4],*/coeft;
	int j,y,output0[4],xx;
	__s32 coefa0,coefa1,inNext[4],output[4];
	int stride = 4*width;

	//边界处理;
	coeft = coefs->cprev;
	
	prevIn[0] = *(in+0);
	prev2Out[0] = mul(prevIn[0],coeft);
	prevOut[0] = prev2Out[0];

	prevIn[1] = *(in+1);
	prev2Out[1] = mul(prevIn[1],coeft);
	prevOut[1] = prev2Out[1];

	prevIn[2] = *(in+2);
	prev2Out[2] = mul(prevIn[2],coeft);
	prevOut[2] = prev2Out[2];

	prevIn[3] = *(in+3);
	prev2Out[3] = mul(prevIn[3],coeft);
	prevOut[3] = prev2Out[3];

	//
	coefa0 = coefs->a0;
	coefa1 = coefs->a1;
	
	y = height;
	for (; y > 0; y--) 
	{
		temp1[0] = prevOut[0];
		prevOut[0] = sub(add(mul(*(in+0),coefa0),mul(prevIn[0],coefa1)),add(mul(prevOut[0],coefs->b1),mul(prev2Out[0],coefs->b2)));
		prev2Out[0] = temp1[0];
		prevIn[0] = *(in+0);
		*(temp+0) = prevOut[0];
		
		temp1[0] = prevOut[1];
		prevOut[1] = sub(add(mul(*(in+1),coefa0),mul(prevIn[1],coefa1)),add(mul(prevOut[1],coefs->b1),mul(prev2Out[1],coefs->b2)));
		prev2Out[1] = temp1[0];
		prevIn[1] = *(in+1);
		*(temp+1) = prevOut[1];

		temp1[0] = prevOut[2];
		prevOut[2] = sub(add(mul(*(in+2),coefa0),mul(prevIn[2],coefa1)),add(mul(prevOut[2],coefs->b1),mul(prev2Out[2],coefs->b2)));
		prev2Out[2] = temp1[0];
		prevIn[2] = *(in+2);
		*(temp+2) = prevOut[2];

		temp1[0] = prevOut[3];
		prevOut[3] = sub(add(mul(*(in+3),coefa0),mul(prevIn[3],coefa1)),add(mul(prevOut[3],coefs->b1),mul(prev2Out[3],coefs->b2)));
		prev2Out[3] = temp1[0];
		prevIn[3] = *(in+3);
		*(temp+3) = prevOut[3];

		in += stride ; temp += 4;
	}

	//bottom-to-top pass
	in -= stride;
	temp -=4;
	out += width*(height-1);

	//边界处理;
	coeft = coefs->cnext;
	
	prevIn[0] = *(in+0);
	prev2Out[0] = mul(prevIn[0],coeft);
	prevOut[0] = prev2Out[0];
	currIn[0] = prevIn[0];

	prevIn[1] = *(in+1);
	prev2Out[1] = mul(prevIn[1],coeft);
	prevOut[1] = prev2Out[1];
	currIn[1] = prevIn[1];

	prevIn[2] = *(in+2);
	prev2Out[2] = mul(prevIn[2],coeft);
	prevOut[2] = prev2Out[2];
	currIn[2] = prevIn[2];

	prevIn[3] = *(in+3);
	prev2Out[3] = mul(prevIn[3],coeft);
	prevOut[3] = prev2Out[3];
	currIn[3] = prevIn[3];

	//加载系数;
	coefa0 = coefs->a2;
	coefa1 = coefs->a3;

	for (y = height-1; y >= 0; y--) 
	{
		
		temp1[0] = prevOut[0];
		prevOut[0] = sub(add(mul(currIn[0],coefa0),mul(prevIn[0],coefa1)),add(mul(prevOut[0],coefs->b1),mul(prev2Out[0],coefs->b2)));
		prev2Out[0] = temp1[0];
		prevIn[0] = currIn[0];
		currIn[0] = *(in+0);
		output[0] = add(*(temp+0),prevOut[0]);
		output0[0] = (int)(output[0]>>8); //fixed to int

		
		temp1[0] = prevOut[1];
		prevOut[1] = sub(add(mul(currIn[1],coefa0),mul(prevIn[1],coefa1)),add(mul(prevOut[1],coefs->b1),mul(prev2Out[1],coefs->b2)));
		prev2Out[1] = temp1[0];
		prevIn[1] = currIn[1];
		currIn[1] = *(in+1);
		output[1] = add(*(temp+1),prevOut[1]);
		output0[1] = (int)(output[1]>>8); //fixed to int

		temp1[0] = prevOut[2];
		prevOut[2] = sub(add(mul(currIn[2],coefa0),mul(prevIn[2],coefa1)),add(mul(prevOut[2],coefs->b1),mul(prev2Out[2],coefs->b2)));
		prev2Out[2] = temp1[0];
		prevIn[2] = currIn[2];
		currIn[2] = *(in+2);
		output[2] = add(*(temp+2),prevOut[2]);
		output0[2] = (int)(output[2]>>8); //fixed to int

		temp1[0] = prevOut[3];
		prevOut[3] = sub(add(mul(currIn[3],coefa0),mul(prevIn[3],coefa1)),add(mul(prevOut[3],coefs->b1),mul(prev2Out[3],coefs->b2)));
		prev2Out[3] = temp1[0];
		prevIn[3] = currIn[3];
		currIn[3] = *(in+3);
		output[3] = add(*(temp+3),prevOut[3]);
		output0[3] = (int)(output[3]>>8); //fixed to int
	
		xx = CLAMP(output0[3],0x0,0xff)<<24;
		xx += CLAMP(output0[2],0x0,0xff)<<16 ;
		xx += CLAMP(output0[1],0x0,0xff)<<8 ;
		xx += CLAMP(output0[0],0x0,0xff);
		//*out = *(unsigned int*)&xx;
		*out = xx;
		in -= stride; out -= width; 
		temp -=4;
	}
}

__s32 GaussianBlur(int width, int height,int stride,float sigma, int* in, int* out)
{
	int i;
	int buffer_size_each_thread = 4 * max(width,height);
	int threads_num = THREADS_NUM;
	int buffer_size = stride*height;
	int temp_buff_length=0;
	int buffers_buff_length=0;
	__s32 *temp = NULL;
	__s32 *buffers = NULL;
	
	temp_buff_length = buffer_size_each_thread*threads_num*sizeof(__s32);
	//buffers_buff_length = buffer_size *4*sizeof(__s32);
	buffers_buff_length = buffer_size*sizeof(__s32);

	__msg("temp_buff_length=%d\n",temp_buff_length);
	__msg("buffers_buff_length=%d\n",buffers_buff_length);

	temp = (__s32*)palloc((temp_buff_length+1023)/1024,0);
	buffers = (__s32*)palloc((buffers_buff_length+1023)/1024,0);
	__msg("temp=%x\n",temp);
	__msg("buffers=%x\n",buffers);
	
	//__s32* temp = (__s32*)malloc(buffer_size_each_thread*threads_num*sizeof(__s32));
	//__s32* buffers = (__s32*) malloc((buffer_size *4*sizeof(__s32)));
	
	//__msg("here");

	//计算系数;
	__msg("iir_coefs=%x\n",iir_coefs);
	CaculateCoefs(sigma,&iir_coefs);
	__msg("here\n");
	i = height;
	for (; i > 0; i--)
	{
		GaussHorizontal(&iir_coefs,width,height,temp,&in[(height-i)*width],&buffers[(height-i)*width*4]);
	}
	__msg("here\n");
	//垂直方向模糊;
	i = width;
	for (; i > 0; i--)
	{
		GaussVertical(&iir_coefs,width,height,temp,&buffers[(width-i)*4],&out[width-i]);
	}

	//free(temp);
	//free(buffers);
	pfree(temp,(temp_buff_length+1023)/1024);
	pfree(buffers,(buffers_buff_length+1023)/1024);

	return EPDK_OK;
	
}


void superFastBlur(unsigned char *pix, int w, int h, int radius)
{
  int div;
  int wm, hm, wh;
  int *vMIN, *vMAX;
  unsigned char *r, *g, *b, *dv;
  int rsum, gsum, bsum, x, y, i, p, p1, p2, yp, yi, yw;

  if (radius < 1) return;

  wm = w - 1;
  hm = h - 1;
  wh = w * h;
  div = radius + radius + 1;
  vMIN = (int *)malloc(sizeof(int) * max(w, h));
  vMAX = (int *)malloc(sizeof(int) * max(w, h));
  r = (unsigned char *)malloc(sizeof(unsigned char) * wh);
  g = (unsigned char *)malloc(sizeof(unsigned char) * wh);
  b = (unsigned char *)malloc(sizeof(unsigned char) * wh);
  dv = (unsigned char *)malloc(sizeof(unsigned char) * 256 * div);

  for (i = 0; i < 256 * div; i++)
    dv[i] = (i / div);

  yw = yi = 0;

  for (y = 0; y < h; y++)
  {
    rsum = gsum = bsum = 0;
    for (i = -radius; i <= radius; i++)
    {
      p = (yi + min(wm, max(i, 0))) * 4;
      bsum += pix[p+3];
      gsum += pix[p + 2];
      rsum += pix[p + 1];
    }
    for (x = 0; x < w; x++)
    {
      r[yi] = dv[rsum];
      g[yi] = dv[gsum];
      b[yi] = dv[bsum];

      if (y == 0)
      {
        vMIN[x] = min(x + radius + 1, wm);
        vMAX[x] = max(x - radius, 0);
      }
      p1 = (yw + vMIN[x]) * 4;
      p2 = (yw + vMAX[x]) * 4;

      bsum += pix[p1+3] - pix[p2+3];
      gsum += pix[p1 + 2] - pix[p2 + 2];
      rsum += pix[p1 + 1] - pix[p2 + 1];

      yi++;
    }
    yw += w;
  }

  for (x = 0; x < w; x++)
  {
    rsum = gsum = bsum = 0;
    yp = -radius * w;
    for (i = -radius; i <= radius; i++)
    {
      yi = max(0, yp) + x;
      rsum += r[yi];
      gsum += g[yi];
      bsum += b[yi];
      yp += w;
    }
    yi = x;
    for (y = 0; y < h; y++)
    {
      pix[yi * 4+3] = dv[bsum];
      pix[yi * 4 + 2] = dv[gsum];
      pix[yi * 4 + 1] = dv[rsum];

      if (x == 0)
      {
        vMIN[y] = min(y + radius + 1, hm) * w;
        vMAX[y] = max(y - radius, 0) * w;
      }
      p1 = x + vMIN[y];
      p2 = x + vMAX[y];

      rsum += r[p1] - r[p2];
      gsum += g[p1] - g[p2];
      bsum += b[p1] - b[p2];

      yi += w;
    }
  }

  free(r);
  free(g);
  free(b);

  free(vMIN);
  free(vMAX);
  free(dv);
}


int gauss_bmp_write_ex(bmp_t_ex *bmp, ES_FILE *out) {
	int len=0;

	len += fwrite(&(bmp->file_head),1, sizeof(bmp_file_head_t), out);
	__msg("len=%d..\n",len);
	len += fwrite(&(bmp->info_head),1, sizeof(bmp_info_head_t), out); 
	__msg("len=%d..\n",len);
	__msg("bmp->info_head.biSizeImage=%d..\n",bmp->info_head.biSizeImage);
	//len += fwrite(bmp->data, 1,bmp->info_head.biSizeImage, out); 
	len += fwrite(bmp->data, bmp->info_head.biSizeImage,1, out); 
	__msg("len=%d..\n",len);

	
	return len;
	 
}



int GaussBlur_test(void)  
{  
    char *InputName, *OutputName;  //输入输出图像文件名变量
	ES_FILE *InputFile, *OutputFile;  //输入输出图像文件
	bmp_t_ex *bmp = NULL, *blur = NULL;
	__s32 width,height;
	int BytesPerLine;
    HBMP  hbmp = 0;
    HBMP_i_t hbmp_buf;
	char *pic_buf=NULL;
	__u32 file_len=0;
	float sigma = SIGMA;
  //  InputName = "c:\\boot_ui\\screen_scroll_bg1.bmp"; OutputName = "f:\\output.bmp";
	
    InputName = "f:\\input.bmp"; OutputName = "f:\\output.bmp";
	
    hbmp = open_bmp(InputName, &hbmp_buf);

    OutputFile = fopen(OutputName, "wb"); //打开并创建输出文件
    __here__
	if (OutputFile == NULL) 
	{
		__msg("Can't open %s\n", OutputName);
		return 1;
	}
    __here__
	
	blur = (bmp_t_ex *)malloc(sizeof(bmp_t_ex));

	width = get_width(hbmp); 
	height = get_height(hbmp);

	file_len = width*height*4;
	blur->data = esMEMS_Palloc((file_len+1023)/1024, 0);

	
	get_data(hbmp,blur->data);
	get_file_head(hbmp,&blur->file_head);
	get_info_head(hbmp,&blur->info_head);
	__msg("blur->file_head.bfType[0]=%c\n",blur->file_head.bfType[0]);
	__msg("blur->file_head.bfType[1]=%c\n",blur->file_head.bfType[1]);
	__msg("blur->file_head.bfSize=%d\n",blur->file_head.bfSize);
	__msg("blur->file_head.bfReserved=%d\n",blur->file_head.bfReserved);
	__msg("blur->file_head.bfOffBits=%d\n",blur->file_head.bfOffBits);


	__msg("blur->info_head.biWidth=%d\n",blur->info_head.biWidth);
	__msg("blur->info_head.biHeight=%d\n",blur->info_head.biHeight);
	__msg("blur->info_head.biBitCount=%d\n",blur->info_head.biBitCount);
	__msg("blur->info_head.biSizeImage=%d\n",blur->info_head.biSizeImage);

	BytesPerLine = ((32 * width + 31) >> 5) << 2;
	GaussianBlur(width,height,BytesPerLine,SIGMA,(int *)blur->data,(int *)blur->data);
	
	
	__msg("Width: %ld\n",blur->info_head.biWidth);
 	__msg("Height: %ld\n", blur->info_head.biHeight);
 	__msg("BitCount: %d\n\n", (int)blur->info_head.biBitCount);
 	__msg("SizeImage: %d\n\n", (int)blur->info_head.biSizeImage);
 	__msg("OffBits: %d\n\n", (int)blur->file_head.bfOffBits);

	
    gauss_bmp_write_ex(blur, OutputFile);//将处理后数据存放到新建的文件中 
    fclose(OutputFile); //关闭文件
    close_bmp(hbmp);//释放存放读取数据的内存
    free(blur);//释放处理后图像数据的内存
    
    return 0;
}  








#endif


