#ifndef __CAUSSBLUR_H__
#define __CAUSSBLUR_H__


#if 0
#define malloc(size)           esMEMS_Malloc(0, size)
#define free(p)                esMEMS_Mfree(0, p)
#define realloc(p, size)       esMEMS_Realloc(0, p, size)
#define calloc(n, m)           esMEMS_Calloc(0, n, m)


#define fopen(filename, mode)             eLIBs_fopen(filename, mode)
#define fclose(stream)                    eLIBs_fclose(stream)
#define fread(ptr, size, nmemb, stream)   eLIBs_fread(ptr, size, nmemb, stream)
#define fwrite(ptr, size, nmemb, stream)  eLIBs_fwrite(ptr, size, nmemb, stream)
#define fseek(stream, offset, whence)     eLIBs_fseek(stream, offset, whence)
#define ftell(stream)                     eLIBs_ftell(stream)
#define ferror(stream)                    eLIBs_ferror(stream)
#define ferrclr(stream)                   eLIBs_ferrclr(stream)
#define remove(filename)                  eLIBs_remove(filename)





#define SQRT_2PI 2.506628274631
//自定义数据类型
typedef unsigned long       DWORD; //四个字节
typedef int                 BOOL;
typedef unsigned char       BYTE; //一个字节
typedef unsigned short      WORD; //一个字节


//位图信息头结构体定义
typedef struct tagBITMAPINFOHEADER
{
   char   bfType[2];  //两个字节
   __u32  bfileSize;
   __u32  bfReserved;
   __u32  bOffBits;
   __u32  biSize;   //4个字节
   __u32   biWidth;  //4个字节
   __s32   biHeight;
   __u16   biPlanes;
   __u16   biBitCount;
   __u32  biCompression;
   __u32  biSizeImage;
   __u32   biXPelsPerMeter;
   __u32   biYPelsPerMeter;
   __u32 biClrUsed;
   __u32 biClrImportant;
} __attribute__((packed))BITMAPINFOHEADER,*PBITMAPINFOHEADER;  //字节对齐

int GaussBlur_test(void);  



#else
#define Q_VALUE	8
#define Qx	8
#define Qy	8
#define Qz	8

#define SIGMA  4.0

typedef struct tag_iir_coefs   //IIR递归高斯滤波系数
{
	__s32 a0;
	__s32 a1;
	__s32 a2;
	__s32 a3;
	__s32 b1;
	__s32 b2; 
	__s32 cprev;
	__s32 cnext;

}iir_coefs_t;

#define CLAMP(x,min_v,max_v) ((x < min_v) ? min_v : ((x > max_v) ? max_v : x))
#define THREADS_NUM 4
#define malloc(size)                      esMEMS_Malloc(0, size)
#define free(memblock)                    esMEMS_Mfree(0, memblock)

#define palloc(npage, mode) esMEMS_Palloc(npage, mode)
#define pfree(pblk, npage) esMEMS_Pfree(pblk, npage)


#define max( x, y )          			( (x) > (y) ? (x) : (y) )
#define min( x, y )          			( (x) < (y) ? (x) : (y) )


#define fopen(filename, mode)             eLIBs_fopen(filename, mode)
#define fclose(stream)                    eLIBs_fclose(stream)
#define fread(ptr, size, nmemb, stream)   eLIBs_fread(ptr, size, nmemb, stream)
#define fwrite(ptr, size, nmemb, stream)  eLIBs_fwrite(ptr, size, nmemb, stream)
#define fseek(stream, offset, whence)     eLIBs_fseek(stream, offset, whence)
#define ftell(stream)                     eLIBs_ftell(stream)
#define ferror(stream)                    eLIBs_ferror(stream)
#define ferrclr(stream)                   eLIBs_ferrclr(stream)
#define remove(filename)                  eLIBs_remove(filename)


int GaussBlur_test(void); 
__s32 GaussianBlur(int width, int height,int stride,float sigma, int* in, int* out);
void superFastBlur(unsigned char *pix, int w, int h, int radius);

#endif


#endif

