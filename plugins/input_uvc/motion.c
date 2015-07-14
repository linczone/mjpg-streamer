/*************************************************************************
	> File Name: motion.c
	> Author: chad
	> Mail: linczone@163.com 
	> Created Time: 2015年07月14日 星期二 15时57分31秒
    > 本项目主要针对嵌入式linux环境，为了提高系统运行效率，该运动检测模块
 直接集成在input_uvc.so中。通过命令行配制项控制运行检测的参数，处理完成的
 图像直接压缩为jpeg格式传递到输出模块。
    > 运动检测的原理是：YUV图像采集 -> 提取Y分量生成单通道灰度图像 -> 与参考帧
 执行图像减法运算 -> 噪声过滤，二值化处理 -> 图像侵蚀处理 -> 图像膨胀处理 -> 计算
 图像变化量 -> 超过运动量阈值则计算运动位置 -> 运动部分使用矩形框标识 -> 添加
 时标等文本信息 -> jpeg压缩 -> 图像输出。
 ************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alg.h"
#include "draw.h"

#define uchar unsigned char 
#define uint unsigned int


typedef struct SMOTION{
	uchar *background;  //存储单通道的灰度图
	uchar *motion;      //运动图，某个像素值不为0说明是运动部分
	uchar *binpic;      //二值化图像
	uchar noise;        //噪声值
	uchar flag;         //图像是否变化的标志
	uint  threshold;    //阈值
	uint  width;
	uint  height;
	uint  timecount;    //运动保持计时器
}SMOTION;

#define MAX_TIMECOUNT   10

static int  MAX_NOISE=20;       //背景噪声值
static int  MAX_THRESHOLD=100;
static unsigned char *gBuf;     //该buf平均分三段，SMOTION结构中的background，motion，motion动态指向某一段
static SMOTION motion;
static int flag_init = -1;      //初始化标志 -1未初始化 other 初始化

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))
#define NORM               100
#define ABS(x)             ((x) < 0 ? -(x) : (x))
#define DIFF(x, y)         (ABS((x) - (y)))
#define NDIFF(x, y)        (ABS(x) * NORM/(ABS(x) + 2 * DIFF(x,y)))

/*
 *   函数名： convert_yuv_to_rgb_buffe
 * ----------------------------------------------
 *   yuv422 格式图像转换为 rgb24 格式
 *
 *   width : 图像宽度
 *   height: 图像高度
 */
void convert_yuv_to_rgb_buffer(uchar *yuv, uchar *rgb, uint width, uint height)
{
	int x,z=0;

	for (x = 0; x < width * height; x++) {
		int r, g, b;
		int y, u, v;

		if(!z) {
			y = yuv[0] << 8;
		} else {
			y = yuv[2] << 8;
		}
		
		u = yuv[1] - 128;
		v = yuv[3] - 128;

		r = (y + (359 * v)) >> 8;
		g = (y - (88 * u) - (183 * v)) >> 8;
		b = (y + (454 * u)) >> 8;

		*(rgb++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
		*(rgb++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
		*(rgb++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);

		if(z++) {
			z = 0;
			yuv += 4;
		}
	}
}
/*
 *  函数名： convert_yuv_to_gray0
 * --------------------------------------------------
 *  yuv422 转换为单通道灰度图像
 *
 *  width   : 图像宽度
 *  height  ：图像高度
 */
int convert_yuv_to_gray0(uchar *yuv, uchar *gray, uint width, uint height)
{
	uint in, out = 0;
	int y0, y1;
	for(in = 0; in < width * height * 2; in += 4) {

		y0 = yuv[in + 0];
		y1 = yuv[in + 2];

		gray[out++] = y0;
		gray[out++] = y1;
	}
	return 0;
}

/*
 *  函数名： motion_init
 *  ------------------------------------------------
 *  运动检测模块参数初始化，该函数需要在模块调用前首先调用
 * 
 *  width   : 图像宽度
 *  height  : 图像高度
 */
int motion_init( uint width, uint height )
{
	memset( &motion, 0, sizeof(motion));
	gBuf = (uchar*)calloc( 1, 3*width * height );
	if( gBuf == NULL) {
		printf("no enough mem ,malloc fail!\n");
		return -1;
	}

	motion.background = gBuf;
	motion.motion = gBuf + width * height;
	motion.binpic = gBuf + 2 * width * height;

	motion.flag = 0;
	motion.timecount = MAX_TIMECOUNT;
	motion.threshold = MAX_THRESHOLD;
	motion.noise = MAX_NOISE;
	return 0;
}
/*
 *  函数名：motion_destroy
 * ---------------------------------
 *  注销资源
 */
int motion_destroy( void )
{
	if( gBuf ) {
		free( gBuf );
		gBuf = NULL;
		motion.background = NULL;
		motion.binpic = NULL;
		motion.motion = NULL;
	}

	return 0;
}
/* 函数名: erode9
 * -------------------------------------------
 * 3x3 box 侵蚀 
 *
 * img      : 图像数据，直接修改输入数据
 * width    : 图像宽度
 * height   : 图像高度
 * buffer   : 3*width 大小的临时buf
 * flag     : 0 or 255 表示背景颜色，比如图像为白底，则flag=0
 */
static int erode9(unsigned char *img, int width, int height, void *buffer, unsigned char flag)
{    
	int y, i, sum = 0;    
	char *Row1,*Row2,*Row3;    
	Row1 = buffer;    
	Row2 = Row1 + width;    
	Row3 = Row1 + 2*width;    
	memset(Row2, flag, width);    
	memcpy(Row3, img, width);    
	for (y = 0; y < height; y++) {        
		memcpy(Row1, Row2, width);        
		memcpy(Row2, Row3, width);        
		if (y == height - 1)            
			memset(Row3, flag, width);        
		else            
			memcpy(Row3, img+(y + 1) * width, width);        
		for (i = width-2; i >= 1; i--) {            
			if (Row1[i-1] == 0 ||                
				Row1[i]   == 0 ||                
				Row1[i+1] == 0 ||                
				Row2[i-1] == 0 ||                
				Row2[i]   == 0 ||                
				Row2[i+1] == 0 ||                
				Row3[i-1] == 0 ||                
				Row3[i]   == 0 ||                
				Row3[i+1] == 0)                
				img[y * width + i] = 0;            
			else                
				sum++;        
		}        
		img[y * width] = img[y * width + width - 1] = flag;    
	}    
	return sum;
}

/* 函数名: dilate9
 * -------------------------------------------
 * 3x3 box 膨胀
 *
 * img      : 图像数据，直接修改输入数据
 * width    : 图像宽度
 * height   : 图像高度
 * buffer   : 3*width 大小的临时buf
 */
static int dilate9(unsigned char *img, int width, int height, void *buffer)
{    
	/* - row1, row2 and row3 represent lines in the temporary buffer      
	 * - window is a sliding window containing max values of the columns     
	 *   in the 3x3 matrix     
	 * - widx is an index into the sliding window (this is faster than      
	 *   doing modulo 3 on i)     
	 * - blob keeps the current max value     
	 */    
	int y, i, sum = 0, widx;    
	unsigned char *row1, *row2, *row3, *rowTemp,*yp;    
	unsigned char window[3], blob, latest;    
	/* Set up row pointers in the temporary buffer. */    
	row1 = buffer;    
	row2 = row1 + width;    
	row3 = row2 + width;    
	/* Init rows 2 and 3. */    
	memset(row2, 0, width);    
	memcpy(row3, img, width);    
	/* Pointer to the current row in img. */    
	yp = img;        
	for (y = 0; y < height; y++) {        
		/* Move down one step; row 1 becomes the previous row 2 and so on. */
		rowTemp = row1;        
		row1 = row2;        
		row2 = row3;        
		row3 = rowTemp;        
		/* If we're at the last row, fill with zeros, otherwise copy from img. */        
		if (y == height - 1)            
			memset(row3, 0, width);        
		else            
			memcpy(row3, yp + width, width);                
		/* Init slots 0 and 1 in the moving window. */        
		window[0] = MAX3(row1[0], row2[0], row3[0]);        
		window[1] = MAX3(row1[1], row2[1], row3[1]);        
		/* Init blob to the current max, and set window index. */        
		blob = MAX2(window[0], window[1]);        
		widx = 2;        
		/* Iterate over the current row; index i is off by one to eliminate 
		 * a lot of +1es in the loop.         
		 */        
		for (i = 2; i <= width - 1; i++) {            
			/* Get the max value of the next column in the 3x3 matrix. */   
			latest = window[widx] = MAX3(row1[i], row2[i], row3[i]);    
			/* If the value is larger than the current max, use it.	Otherwise,             
			* calculate a new max (because the new value may not be the max.          
			*/            
			if (latest >= blob)                
				blob = latest;            
			else                
				blob = MAX3(window[0], window[1], window[2]);            
			/* Write the max value (blob) to the image. */            
			if (blob != 0) {                
				*(yp + i - 1) = blob;                
				sum++;            
			}            
			/* Wrap around the window index if necessary. */            
			if (++widx == 3)                
				widx = 0;        
		}        
		/* Store zeros in the vertical sides. */        
		*yp = *(yp + width - 1) = 0;        
		yp += width;    
	}        
	return sum;
}
/*
 *  函数名：pic_mark
 * ----------------------------------------------------------------
 *  标识变化区域，同时该区域还是一个计数器，每次更新都要将标记位置减1, 
 *  当标志位置减为0时，说明该位置已经不再运动,由于拍照时间很快，如果单色
 *  物体在摄像区域内做平移运动时，如果不使用计数器方式，会造成检测到的运
 *  动部位只存在与交界区域。为了克服上面的问题，使用延迟计数方式，保证能
 *  标识出完整的变化部分.
 *
 *  motion  : 输入的图像
 *  mark    : 掩码图像，掩码图像与输入的运动图像为尺寸相同的单通道图像
 *  size    : 图像尺寸 = width * height
 */
int pic_mark( unsigned char *motion, unsigned char *mark, unsigned int size )
{
    int i=0;
	int cnt = 0;
    
	while( i != size ) {
	    if(motion[i] > 0) motion[i]--;
	    if( mark ) {
		    if( mark[i] ) {
			    cnt++;
			    motion[i] = BRIGHTNESS;
		    }
		}
		i++;
	}

	return cnt;
}
//标识出变化区域，目前是通过改变变化区域的亮度进行标识
void pic_add( unsigned char *rgbimg, unsigned char *mark, unsigned int size )
{
    int i=0;
    int tmp;
    
	while( i != size ) {
		if( mark[i] ) {
		    tmp = rgbimg[i*3] + mark[i]<<1;
	        rgbimg[i*3] = tmp>255?255:tmp;
		}
		i++;
	}
}
  
//图像减法运算，返回变化的像素个数,结果存储在img1中
int pic_subtraction( unsigned char *img1, unsigned char *img2, unsigned int size )
{
	int i=0;
	//int cnt = 0;

	while( i != size ) {
		//像素值不能出现反转，求绝对值
		img2[i] = img1[i] > img2[i]?img1[i] - img2[i]:img2[i] - img1[i];
		i++;
	}

	return 0;
}
//图像二值化处理，小于noise 像素置0 大于noise 像素置 255，返回变化像素个数
int pic_binmap( unsigned char *img, unsigned char *binimg, unsigned char noise, unsigned int size )
{
	int i=0;
	int cnt = 0;

	while( i != size ) {
		binimg[i] = img[i] > noise?255:0;
		if( binimg[i] )
			cnt++;
		i++;
	}

	return cnt;
}
//图像二值化处理，小于noise 像素置0 大于noise 像素置 255，返回变化像素个数
int pic_noise_filter( unsigned char *img, unsigned char noise, unsigned int size )
{
	int i=0;
	int cnt = 0;

	while( i != size ) {
		if( img[i] < noise ) {
			img[i] = 0;
		} else {
			cnt++;
		}
		i++;
	}

	return cnt;
}
int noise_tune( unsigned char *img, unsigned int width, unsigned int height )
{
	long sum = 0;
	int cnt = 0;
	int i;
	unsigned char pmax,pmin;

	pmax = pmin = 0;
	for(i=0;i!=width*height;i++)
	{
		if( img[i] ) {
			cnt++;
			sum += img[i];
			pmax = img[i] > pmax?img[i]:pmax;
		}		
	}

	int avg = sum / cnt;

	printf("pmax = %d cnt = %d avg=%d\n",pmax,cnt,avg);
	printf("+++++++++++++++++++++++++\n");

	return avg;
}
void param_init( int noise, int threshold )
{
	if( noise > 0 )
		MAX_NOISE = noise;
	if( threshold > 0 )
		MAX_THRESHOLD = threshold;
}
int motion_check( unsigned char *yuv, unsigned char *out, unsigned int width, unsigned int height )
{
	int change = 0;
	
	if( 0 > flag_init ) {
		if( !motion_init( width , height ) ) {
			flag_init = 0;//初始化完成
			motion.width = width;
			motion.height = height;
			printf("motion.background saved\n");
			initialize_chars();
			//保存背景图片
			convert_yuv_to_gray0( yuv, motion.background, width, height );
			return 0;
		} else {
			printf("motion init fail!\n");
			return -1;
		}
	}
	convert_yuv_to_rgb_buffer( yuv, out, width, height);
	convert_yuv_to_gray0( yuv, motion.motion, width, height );
	pic_subtraction( motion.motion, motion.background, width * height );
	
	uchar *tmp = motion.background;
	motion.background = motion.motion;//更新背景
	motion.motion = tmp;
	char timebuf[50]={0};
	mystrtime( timebuf, "%Y-%m-%d %H:%M:%S" );

	draw_text( out, width-strlen(timebuf), height-10, width, timebuf, 0 );
	
	//运动区域更新
	//pic_mark( motion.motion, NULL, width * height );
	//noise_tune( motion.motion, width, height);
	change = pic_noise_filter( motion.motion, motion.noise, width * height );
	if( change > motion.threshold ) {//如果去除噪声后改变量依然大于运动检测阈值则进行下一步运算
		struct coord crd;
		unsigned char *tmpbuf;
		tmpbuf = (unsigned char *)calloc(1,width*3);
		
		printf("-------------------------------\n");
		printf("noise=%d\n",motion.noise);
		printf("change=%d\n",change);
		printf("threshold=%d\n",motion.threshold);
		//腐蚀处理
		printf("erode9=%d\n",erode9( motion.motion, width, height, tmpbuf, 0 ));
		//膨胀处理
		printf("dilate9=%d\n",dilate9( motion.motion, width, height, tmpbuf ));
		alg_locate_center_size( motion.motion, width, height,&crd );
		alg_draw_location( &crd, out, width, 1 );
		//标识出变化区域
		//pic_add( out, motion.motion, width * height );
		free(tmpbuf);
		printf("===================\n");
		return 0xaa;//检测到运动
	}

	
	return 0;
}

