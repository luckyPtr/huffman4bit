/**
 * @file badapple_decode.c
 * @brief Minimal usage example: decode every compressed frame and compare
 *        it against the original.
 *
 * Typical project flow:
 *   1. PC:  h4b_encoder -i my_images.c -o mydata -k 1
 *           -> mydata.c / mydata.h (compressed blobs + Huffman tables)
 *   2. MCU: #include them, call h4b_decode() into a frame buffer.
 *
 * Build: make && make run     (uses the shipped shared asset ../assets/badapple_h4b.*)
 */

#include <stdio.h>
#include <string.h>
#include "huffman4bit.h"

/* shared resources, found via -I../assets (examples/assets/) */
#define IMG_BAD_APPLE RAW_IMG_BAD_APPLE   /* rename the originals: the compressed
                                           * pointer table keeps the source name */
#include "badapple.c"           /* IMG_BAD_APPLE[][1024] (originals)    */
#undef IMG_BAD_APPLE
#include "badapple_h4b.h"       /* H4B_BADAPPLE_* macros + extern decls */
#include "badapple_h4b.c"       /* tables + compressed blob arrays      */

#define FRAME_SIZE 1024
/* frame count comes straight from the sized extern in badapple_h4b.h */
#define FRAME_COUNT (sizeof(IMG_BAD_APPLE) / sizeof(IMG_BAD_APPLE[0]))

int main(void)
{
    static unsigned char frame[FRAME_SIZE];
    unsigned int fails = 0;

    for (unsigned int i = 0; i < FRAME_COUNT; i++) {
        unsigned int len = 0;

        h4b_decode(IMG_BAD_APPLE[i],
                   IMG_BAD_APPLE_LEN[i],
                   h4b_badapple_h4b_tables,
                   frame, &len);

        if (len != FRAME_SIZE ||
            memcmp(frame, RAW_IMG_BAD_APPLE[i], FRAME_SIZE) != 0) {
            fails++;
        }
    }

    printf("decoded %u frames, %u mismatch\n",
           (unsigned int)FRAME_COUNT, fails);
    return fails ? 1 : 0;
}
