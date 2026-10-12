/**
 * SSD1306 128x64 monochrome OLED driver (SPI1, hardware).
 *
 * Pins come from the CubeMX labels in main.h:
 *   PA5 = SPI1_SCK, PA7 = SPI1_MOSI, DC = PB1, RST = PC5, CS = PF11.
 *
 * The frame path presets horizontal addressing to the full screen, so a
 * frame is just SSD1306_FRAME_LEN page-format bytes streamed back to back.
 * That is exactly the byte order h4b_decode_stream() emits, so playback is:
 *
 *   ssd1306_frame_begin();
 *   h4b_decode_stream(in, in_len, tables, ssd1306_stream_byte, 0);
 *   ssd1306_frame_end();
 */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#define SSD1306_FRAME_LEN   (SSD1306_WIDTH * SSD1306_HEIGHT / 8)  /* 1024 */

void ssd1306_init(void);
void ssd1306_display_on(uint8_t on);
void ssd1306_set_contrast(uint8_t value);
void ssd1306_invert(uint8_t on);        /* 1: 0 = lit pixel */

void ssd1306_clear(void);
void ssd1306_fill(uint8_t pattern);     /* byte pattern per 8-px page row */

void ssd1306_frame_begin(void);         /* select whole screen, CS low, DC high */
void ssd1306_frame_end(void);           /* CS high */
void ssd1306_write_byte(uint8_t byte);  /* one data byte inside a frame */
void ssd1306_write_data(const uint8_t *buf, uint16_t len);
void ssd1306_blit(const uint8_t *frame);/* one-shot full frame (buffered path) */

/* h4b_emit_fn compatible (see huffman4bit.h) */
void ssd1306_stream_byte(void *ctx, uint8_t byte);

#endif /* SSD1306_H */
