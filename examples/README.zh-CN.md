# 例程

[English](README.md) | 简体中文

每个例程一个独立文件夹,全部共享 [`assets/`](assets/) 里的资源,所有例程都按**仅解码**方式编译(不加 `-DUSE_ENCODE`)

```
examples/
├── assets/              # 下方所有例程的共享资源
│   ├── badapple.c           # 原始帧
│   └── badapple_h4b.*       # 压缩资源,编码器输出
├── badapple_decode/     # 最小用法示例
├── badapple_player/     # 终端播放器(Bad Apple!!,原帧 | 解码帧对照)
└── badapple_stm32/      # STM32F103 + SSD1306 实机播放
```

| 例程 | 内容 |
|---|---|
| [`badapple_decode/badapple_decode.c`](badapple_decode/badapple_decode.c) | 最小集成示例:解码全部 3288 帧并与原帧逐字节比对 |
| [`badapple_player/badapple_player.c`](badapple_player/badapple_player.c) | 终端播放器:原帧与解码帧并排播放,实时显示压缩率与校验结果 |
| [`badapple_stm32/Core/Src/main.c`](badapple_stm32/Core/Src/main.c) | 单片机实机示例:SPI 硬件驱动 SSD1306 循环播放整段视频 |

## 运行 PC 例程

需要 `gcc`(或 clang / mingw)和 `make`:

```bash
cd examples/badapple_decode  && make run      # 解码全部 3288 帧并与原帧比对校验
cd examples/badapple_player  && make run      # 原帧 | 解码帧 并排播放(Ctrl+C 退出)
```

## badapple_stm32:单片机实机例程

STM32F103 工程,演示库在真实硬件上的完整用法:

- 目标板:STM32F103ZET6(LQFP144),OLED 为 SSD1306 128x64,SPI1 硬件外设
- 接线(CubeMX 引脚标签见 [`Core/Inc/main.h`](badapple_stm32/Core/Inc/main.h)):

| OLED 引脚 | MCU 引脚 | 说明 |
|---|---|---|
| SCK | PA5 | SPI1_SCK |
| MOSI | PA7 | SPI1_MOSI |
| DC | PB1 | 数据/命令 |
| RST | PC5 | 复位 |
| CS | PF11 | 片选 |

- 播放逻辑在 [`Core/Src/main.c`](badapple_stm32/Core/Src/main.c) 的
  `badapple_play()`,用 `USE_FRAME_BUFFER` 宏切换两种解码路径:
  - `1`(默认):`h4b_decode()` 解到 1 KB 帧缓冲再整帧上屏(`ssd1306_blit`),
    支持随机访问任意帧,代价是 1 KB RAM;
  - `0`:`h4b_decode_stream()` 解出一个字节就经 `ssd1306_stream_byte` 回调
    直接写 SPI 总线,**零帧缓冲**。
- 15 fps 播放整段视频(约 3 分半),播完自动循环。
