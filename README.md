# huffman4bit

English | [简体中文](README.zh-CN.md)

A 4-bit-nibble Huffman codec with XOR+ZRL preprocessing for OLED bitmap
compression, designed for monochrome OLED image/font data on
resource-constrained MCUs. The decode-only MCU side needs just
**284 B ROM / 0 B RAM** (Cortex-M0, arm-none-eabi-gcc -Os; stream API).

## Repository layout

```
huffman4bit/
├── src/        # Core algorithm library (single .c + .h)
├── examples/   # Demos: PC decode / terminal playback + STM32 on-target demo
└── tools/      # Encoder tool h4b_encoder
```

## Quick start

Compress image arrays with the encoder on the PC, decode only on the MCU.

### PC side: compress with the tool

Grab a prebuilt executable from
[Releases](https://github.com/luckyPtr/huffman4bit/releases) or build it
locally (`cd tools && make`). Its input is a C source file containing `0xXX`
hex byte arrays (OLED bitmaps, font data, etc.):

```bash
./h4b_encoder -i my_images.c -o mydata -v
```

This generates `mydata.c/.h` in the current directory - just add them to the
MCU build. See [tools/README.md](tools/README.md) for options, input format
and generated files.

### MCU side: decode only

`USE_ENCODE` defaults to **0**: add `src/huffman4bit.c` and the generated
`.c/.h` from the previous step to the build. Two decode APIs:

```c
#include "huffman4bit.h"

/* classic buffer API (output buffer must fit the original data) */
unsigned char out[1024];
unsigned int out_len;
h4b_decode(in, in_len, tables, out, &out_len);

/* streaming API: bytes are emitted as they are reconstructed - wire the
 * callback to SSD1306 horizontal-addressing I2C/SPI writes and decode
 * straight to the screen with zero frame buffer */
static void to_screen(void *ctx, uint8_t byte) { oled_data(byte); }
h4b_decode_stream(in, in_len, tables, to_screen, 0);
```

The bundled PC demos (decode verification / terminal playback) run without
building the encoder - see [examples/README.md](examples/README.md).

## Measured compression

| Dataset | h4b | LZ4 | heatshrink | deflate |
|---|---|---|---|---|
| Bad Apple!! video (3288 frames @ 15 fps) | **11.2%** | 18.0% | 20.3% | 16.5% |

## Decoder footprint

Measured on Cortex-M0 (`arm-none-eabi-gcc -Os`):

| Decoder | Code (B) | Static RAM (B) | Output buffer |
|---|---|---|---|
| h4b (stream API) | **284** | **0** | none (write callback) |
| h4b (buffer API) | 244 | 0 | one block of original size |
| LZ4 | 728 | 0 | one block of original size |
| heatshrink | 576 | 376 (256 B sliding window) | none (window built in) |
| deflate | 1740 | 724 | one block of original size |

## License

See [LICENSE](LICENSE).
