
#include "alg.h"


#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) ((x) > (y) ? ((x) > (z) ? (x) : (z)) : ((y) > (z) ? (y) : (z)))

void alg_locate_center_size(unsigned char *imgs, int width, int height, struct coord *cent)
{
    unsigned char *out = imgs;
    int x, y, centc = 0, xdist = 0, ydist = 0;

    cent->x = 0;
    cent->y = 0;
    cent->maxx = 0;
    cent->maxy = 0;
    cent->minx = width;
    cent->miny = height;


	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			if (*(out++)) {
				cent->x += x;
				cent->y += y;
				centc++;
			}
		}
	}
 

    if (centc) {
        cent->x = cent->x / centc;
        cent->y = cent->y / centc;
    }
    
    centc = 0;
    out = imgs;


	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			if (*(out++)) {
				if (x > cent->x)
					xdist += x - cent->x;
				else if (x < cent->x)
					xdist += cent->x - x;

				if (y > cent->y)
					ydist += y - cent->y;
				else if (y < cent->y)
					ydist += cent->y - y;

				centc++;
			}
		}    
	}
    
    
    if (centc) {
        cent->minx = cent->x - xdist / centc * 2;
        cent->maxx = cent->x + xdist / centc * 2;
        cent->miny = cent->y - ydist / centc * 2;
        cent->maxy = cent->y + ydist / centc * 2;
    }

    if (cent->maxx > width - 1)
        cent->maxx = width - 1;
    else if (cent->maxx < 0)
        cent->maxx = 0;

    if (cent->maxy > height - 1)
        cent->maxy = height - 1;
    else if (cent->maxy < 0)
        cent->maxy = 0;
    
    if (cent->minx > width - 1)
        cent->minx = width - 1;
    else if (cent->minx < 0)
        cent->minx = 0;
    
    if (cent->miny > height - 1)
        cent->miny = height - 1;
    else if (cent->miny < 0)
        cent->miny = 0;
    
    cent->width = cent->maxx - cent->minx;
    cent->height = cent->maxy - cent->miny;
    
    cent->y = (cent->miny + cent->maxy) / 2;
    
}

void alg_draw_location(struct coord *cent, unsigned char *imgs, int width, int mode)
{
    unsigned char *out = imgs;
    int x, y;

    if (mode){ 
        int width_miny = width*3 * cent->miny;
        int width_maxy = width*3 * cent->maxy;

        for (x = cent->minx; x <= cent->maxx; x++) {
            int width_miny_x = 3*x + width_miny;
            int width_maxy_x = 3*x + width_maxy;
            out[width_miny_x]=~out[width_miny_x];
            out[width_maxy_x]=~out[width_maxy_x];
			out[width_miny_x+1]=~out[width_miny_x+1];
            out[width_maxy_x+1]=~out[width_maxy_x+1];
			out[width_miny_x+2]=~out[width_miny_x+2];
            out[width_maxy_x+2]=~out[width_maxy_x+2];
        }

        for (y = cent->miny; y <= cent->maxy; y++) {
            int width_minx_y = cent->minx*3 + y * width*3; 
            int width_maxx_y = cent->maxx*3 + y * width*3;
            out[width_minx_y]=~out[width_minx_y];
            out[width_maxx_y]=~out[width_maxx_y];
			out[width_minx_y+1]=~out[width_minx_y+1];
            out[width_maxx_y+1]=~out[width_maxx_y+1];
			out[width_minx_y+2]=~out[width_minx_y+2];
            out[width_maxx_y+2]=~out[width_maxx_y+2];
        }
    }
}


