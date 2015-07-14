

#ifndef _INCLUDE_ALG_H
#define _INCLUDE_ALG_H


struct coord {
    int x;
    int y;
    int width;
    int height;
    int minx;
    int maxx;
    int miny;
    int maxy;
};



void alg_locate_center_size(unsigned char *imgs, int width, int height, struct coord *cent);
void alg_draw_location(struct coord *cent, unsigned char *imgs, int width, int mode);


#endif /* _INCLUDE_ALG_H */
