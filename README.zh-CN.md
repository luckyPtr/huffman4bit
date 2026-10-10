# huffman4bit

[English](README.md) | 简体中文

一个面向 OLED 位图压缩的 4-bit 半字节霍夫曼编解码器(XOR+ZRL预处理),
专为资源受限单片机上的单色 OLED 图片/字库数据设计。单片机端纯解码仅
需**284 B ROM / 0 B RAM** (Cortex-M0,arm-none-eabi-gcc -Os; stream API)

## 仓库结构

```
huffman4bit/
├── src/        # 核心算法库(单个 .c + .h)
├── examples/   # 例程:PC 解码 / 终端播放演示 + STM32 实机演示
└── tools/      # 编码器工具 h4b_encoder
```

## 快速上手

PC 端用编码器压缩图片数组,单片机端仅解码

### PC 端:用工具压缩

编码器可直接从 [Releases](https://github.com/luckyPtr/huffman4bit/releases)
下载预编译可执行文件,或本地构建(`cd tools && make`)。输入内含 `0xXX` 形式的十六进制数组
(OLED 位图、字库数据等)的 C 源文件:

```bash
./h4b_encoder -i my_images.c -o mydata -v
```

在当前目录生成 `mydata.c/.h`,加入单片机构建即可。参数、输入格式与生成
文件详见 [tools/README.zh-CN.md](tools/README.zh-CN.md)。

### 单片机端:仅解码

`USE_ENCODE` 默认为 **0**,把 `src/huffman4bit.c` 与上一步生成的
`.c/.h` 加入构建即可。两种解码 API:

```c
#include "huffman4bit.h"

/* 经典缓冲区 API(输出缓冲区须能容纳原始数据) */
unsigned char out[1024];
unsigned int out_len;
h4b_decode(in, in_len, tables, out, &out_len);

/* 流式 API:解出的字节即时回调输出——如把回调接到 SSD1306 水平寻址的
 * I2C/SPI 写操作上,即可零帧缓冲直接解码上屏 */
static void to_screen(void *ctx, uint8_t byte) { oled_data(byte); }
h4b_decode_stream(in, in_len, tables, to_screen, 0);
```

仓库附带的 PC 例程(解码校验 / 终端播放)无需构建编码器即可直接运行,
见 [examples/README.zh-CN.md](examples/README.zh-CN.md)。

## 实测压缩率

| 数据集 | h4b | LZ4 | heatshrink | deflate |
|---|---|---|---|---|
| Bad Apple!! 视频(3288 帧 @ 15 fps) | **11.2%** | 18.0% | 20.3% | 16.5% |

## 解码端占用

Cortex-M0 实测(`arm-none-eabi-gcc -Os`):

| 解码器 | 代码 (B) | 静态 RAM (B) | 输出缓冲 |
|---|---|---|---|
| h4b(stream API) | **284** | **0** | 无 |
| h4b(buffer API) | 244 | 0 | 原始大小 |
| LZ4 | 728 | 0 | 原始大小 |
| heatshrink | 576 | 376(256 B 滑窗) | 无 |
| deflate | 1740 | 724 | 原始大小 |

## 许可证

见 [LICENSE](LICENSE)。
