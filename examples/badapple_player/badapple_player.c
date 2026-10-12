/**
 * @file badapple_player.c
 * @brief Terminal player: shows the original BadApple frames next to the frames
 *        reconstructed from the huffman4bit-compressed stream, side by side.
 *
 * Prerequisite: the compressed asset is shipped in ../assets/; see
 * ../README.md to regenerate it with the encoder.
 *
 * This translation unit is decode-only (compiled WITHOUT -DUSE_ENCODE) to mirror
 * MCU-side usage of the library.
 *
 * Terminal layout (1:1 pixels via Unicode half-blocks, each text cell = 2 pixels):
 *   line 1      : HUD
 *   lines 2..33 : | original 128x64 | decoded 128x64 |
 *
 * Run:  make run    (Ctrl+C to quit, loops forever)
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32) || defined(_MSC_VER)
  #include <windows.h>
#else
  #include <time.h>
  #include <unistd.h>
#endif

#include "huffman4bit.h"

/* shared resources, found via -I../assets (examples/assets/) */
#define IMG_BAD_APPLE RAW_IMG_BAD_APPLE   /* rename the originals: the compressed
                                           * pointer table keeps the source name */
#include "badapple.c"           /* IMG_BAD_APPLE[][1024] (original frames) */
#undef IMG_BAD_APPLE
#include "badapple_h4b.h"       /* H4B_BADAPPLE_* macros + extern decls    */
#include "badapple_h4b.c"       /* tables + compressed blob arrays         */

#define ORIG_FRAMES  (sizeof(RAW_IMG_BAD_APPLE) / sizeof(RAW_IMG_BAD_APPLE[0]))
#define COMP_FRAMES  ((unsigned int)(sizeof(IMG_BAD_APPLE) / sizeof(IMG_BAD_APPLE[0])))

#define WIDTH        (128U)
#define HEIGHT       (64U)
#define FRAME_SIZE   (1024U)
#define FPS          (15U)
#define INVERT       (0)           /* set to 1 if the picture looks inverted   */

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int s) { (void)s; g_stop = 1; }

/* ------------------------------------------------------------------ */
/*  Platform helpers: wall-clock ms + sleep + terminal init            */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(_MSC_VER)
static unsigned long now_ms(void) { return (unsigned long)GetTickCount(); }
static void sleep_ms(unsigned long ms) { Sleep((DWORD)ms); }
static void term_init(void)
{
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
static unsigned long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)(ts.tv_nsec / 1000000L);
}
static void sleep_ms(unsigned long ms)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000UL);
    ts.tv_nsec = (long)((ms % 1000UL) * 1000000L);
    nanosleep(&ts, NULL);
}
static void term_init(void) { /* POSIX terminals speak ANSI/UTF-8 natively */ }
#endif

/* ------------------------------------------------------------------ */
/*  Pixel access (SSD1306 page-mode: byte = page*128 + col, LSB = top) */
/* ------------------------------------------------------------------ */
static int px(const unsigned char *f, int x, int y)
{
    int b = (f[(y >> 3) * (int)WIDTH + x] >> (y & 7)) & 1;
#if INVERT
    b = !b;
#endif
    return b;
}

/* half-block glyphs, indexed by (top<<1)|bot */
static const char *BLOCKS[4] = {
    " ",                /* 0: 0,0  empty        */
    "\xE2\x96\x84",     /* 1: 0,1  lower half   */
    "\xE2\x96\x80",     /* 2: 1,0  upper half   */
    "\xE2\x96\x88",     /* 3: 1,1  full block   */
};

#define W_PUTS(s) do { const char *_s = (s); size_t _l = strlen(_s); \
                       memcpy(p, _s, _l); p += _l; } while (0)

static void render_frame(unsigned int idx,
                         const unsigned char *orig,
                         const unsigned char *dec,
                         int match,
                         unsigned int comp_bytes)
{
    static char buf[40000];
    char *p = buf;
    unsigned int r, x;

    /* HUD on line 1 (absolute positioning => never scrolls the terminal) */
    p += sprintf(p, "\x1B[1;1H");
    p += sprintf(p,
        "huffman4bit BadApple player | frame %u/%u | orig %uB | comp %uB | "
        "ratio %4.1f%% | decode: %s",
        idx + 1U, COMP_FRAMES, FRAME_SIZE, comp_bytes,
        100.0 * (double)comp_bytes / (double)FRAME_SIZE,
        match ? "OK (lossless)" : "MISMATCH");
    W_PUTS("\x1B[K");

    /* 32 text rows, each covering 2 pixel rows */
    for (r = 0; r < HEIGHT / 2U; r++) {
        p += sprintf(p, "\x1B[%u;1H", r + 2U);

        for (x = 0; x < WIDTH; x++) {
            int t = px(orig, (int)x, (int)(r * 2U));
            int b = px(orig, (int)x, (int)(r * 2U + 1U));
            W_PUTS(BLOCKS[(t << 1) | b]);
        }
        W_PUTS(" \xE2\x94\x82 ");   /* " | " separator (U+2502) */

        for (x = 0; x < WIDTH; x++) {
            int t = px(dec, (int)x, (int)(r * 2U));
            int b = px(dec, (int)x, (int)(r * 2U + 1U));
            W_PUTS(BLOCKS[(t << 1) | b]);
        }
        W_PUTS("\x1B[K");
    }

    fwrite(buf, 1, (size_t)(p - buf), stdout);
    fflush(stdout);
}

int main(void)
{
    static unsigned char dec[FRAME_SIZE];
    unsigned int i;
    unsigned long target_ms;

    if (COMP_FRAMES == 0) {
        fprintf(stderr, "error: no compressed data - regenerate with h4b_encoder (see ../README.md).\n");
        return 1;
    }
    if (ORIG_FRAMES != COMP_FRAMES) {
        fprintf(stderr,
            "warning: IMG_BAD_APPLE has %u frames, compressed data has %u\n",
            (unsigned int)ORIG_FRAMES, COMP_FRAMES);
    }
    term_init();
    signal(SIGINT, on_sigint);

    /* clear screen + hide cursor */
    fputs("\x1B[2J\x1B[?25l", stdout);
    fflush(stdout);

    target_ms = 1000UL / FPS;
    for (;;) {
        for (i = 0; i < COMP_FRAMES; i++) {
            if (g_stop) goto done;

            unsigned long t0 = now_ms();
            unsigned int dec_len = 0;

            h4b_decode(IMG_BAD_APPLE[i],
                       IMG_BAD_APPLE_LEN[i],
                       h4b_badapple_h4b_tables,
                       dec, &dec_len);

            int match = (dec_len == FRAME_SIZE) &&
                        (memcmp(RAW_IMG_BAD_APPLE[i], dec, FRAME_SIZE) == 0);

            render_frame(i, RAW_IMG_BAD_APPLE[i], dec, match,
                         IMG_BAD_APPLE_LEN[i]);

            unsigned long dt = now_ms() - t0;
            if (dt < target_ms) sleep_ms(target_ms - dt);
        }
    }

done:
    fputs("\x1B[?25h\n", stdout);   /* show cursor again */
    fflush(stdout);
    return 0;
}
