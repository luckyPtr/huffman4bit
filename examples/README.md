# Examples

English | [简体中文](README.zh-CN.md)

One folder per example, all sharing the resources in [`assets/`](assets/), and
every example builds **decode-only** (no `-DUSE_ENCODE`).

```
examples/
├── assets/              # shared resources for every example below
│   ├── badapple.c           # source frames
│   └── badapple_h4b.*       # compressed asset, encoder output
├── badapple_decode/     # minimal usage sample
├── badapple_player/     # terminal player (Bad Apple!!, original | decoded)
└── badapple_stm32/      # on-target playback, STM32F103 + SSD1306
```

| Example | What it shows |
|---|---|
| [`badapple_decode/badapple_decode.c`](badapple_decode/badapple_decode.c) | Minimal integration: decode all 3288 frames and compare byte-by-byte against the originals |
| [`badapple_player/badapple_player.c`](badapple_player/badapple_player.c) | Terminal player: original and decoded frames side by side, live compression ratio and verification status |
| [`badapple_stm32/Core/Src/main.c`](badapple_stm32/Core/Src/main.c) | On-target example: loops the whole video on an SSD1306 over hardware SPI |

## Run the PC examples

Requires `gcc` (or clang / mingw) and `make`:

```bash
cd examples/badapple_decode  && make run      # decode all 3288 frames, verify against originals
cd examples/badapple_player  && make run      # original | decoded side-by-side playback (Ctrl+C to quit)
```

## badapple_stm32: on-target example

An STM32F103 project demonstrating the full library usage on real hardware:

- Target board: STM32F103ZET6 (LQFP144), SSD1306 128x64 OLED, hardware SPI1
- Wiring (CubeMX pin labels in [`Core/Inc/main.h`](badapple_stm32/Core/Inc/main.h)):

| OLED pin | MCU pin | Notes |
|---|---|---|
| SCK | PA5 | SPI1_SCK |
| MOSI | PA7 | SPI1_MOSI |
| DC | PB1 | data/command |
| RST | PC5 | reset |
| CS | PF11 | chip select |

- Playback lives in `badapple_play()` in [`Core/Src/main.c`](badapple_stm32/Core/Src/main.c);
  the `USE_FRAME_BUFFER` macro switches between two decode paths:
  - `1` (default): `h4b_decode()` into a 1 KB frame buffer, then blit the whole
    frame (`ssd1306_blit`) - random access to any frame, at the cost of 1 KB RAM;
  - `0`: `h4b_decode_stream()` writes each reconstructed byte straight to the
    SPI bus via the `ssd1306_stream_byte` callback - **zero frame buffer**.
- Plays the whole video at 15 fps (~3.5 minutes), looping automatically when done.
