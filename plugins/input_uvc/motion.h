
#ifndef __motion_H__
#define __motion_H__


/*yuv4:2:2格式转换为rgb24格式*/
//int convert_yuv_to_rgb_pixel(int y, int u, int v);

int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width,unsigned int height);
//yuv 转换为灰度图像
int convert_yuv_to_gray(unsigned char *yuv, unsigned char *rgb, unsigned int width,unsigned int height);
//yuv 转换为灰度图像 单通道
int convert_yuv_to_gray0(unsigned char *yuv, unsigned char *gray, unsigned int width,unsigned int height);

int motion_init( unsigned int width, unsigned int height );
int motion_destroy( void );
//3*3 侵蚀
void erosion( unsigned char *img, unsigned int width, unsigned int height );
//3*3 膨胀
void dilation( unsigned char *img, unsigned int width, unsigned int height );
//标识变化区域，同时该区域还是一个计数器，每次更新都要将标记位置减1 当标志位置减为0时，说明该位置已经不在运动
//由于拍照时间很快，如果单色物体在摄像区域内做平移运动时，如果不使用计数器方式，会造成检测到的运动部位只存在与交界区域。
//为了克服上面的问题，使用延迟计数方式，保证能标识出完整的变化部分
int pic_mark( unsigned char *motion, unsigned char *mark, unsigned int size );
//标识出变化区域，目前是通过改变变化区域的亮度进行标识
void pic_add( unsigned char *rgbimg, unsigned char *mark, unsigned int size );
  
//图像减法运算，返回变化的像素个数,结果存储在img1中
int pic_subtraction( unsigned char *img1, unsigned char *img2, unsigned int size );
//图像二值化处理，小于noise 像素置0 大于noise 像素置 255，返回变化像素个数
int pic_binmap( unsigned char *img, unsigned char *binimg, unsigned char noise, unsigned int size );
int motion_check( unsigned char *yuv, unsigned char *out, unsigned int width, unsigned int height );
void param_init( int noise, int threshold );
#endif


