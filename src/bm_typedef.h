#ifndef __INC_BM_TYPEDEF_H__
#define __INC_BM_TYPEDEF_H__
typedef const unsigned char Img_t;
typedef const struct
{
    Img_t *img;
    unsigned char x;
    unsigned char y;
    unsigned char width;
    unsigned char height;
}ComImg_t;
#define END_OF_IMG (0)
#endif
