# tools — h4b_encoder

[English](README.md) | 简体中文

huffman4bit 的 PC 端编码器:输入含十六进制字节数组(OLED 位图、字库数据
等)的 C 源文件,输出压缩后的 `<name>.c` / `<name>.h`,单片机上用仅解码
库直接消费。

## 构建

需要 C 编译器(gcc / clang / mingw)和 `make`。所有构建产物都进 `build/`:

```bash
make          # -> build/h4b_encoder(.exe)
make clean    # 删除 build/
```

## 用法

```bash
./build/h4b_encoder -i ../examples/assets/badapple.c -o badapple_h4b -k 2 -v
# 在当前目录生成 ./badapple_h4b.c 和 ./badapple_h4b.h
```

| 参数 | 必填 | 说明 |
|---|---|---|
| `-i <file>` | 是 | 输入的 C 源文件(含 `0xXX` 十六进制数组) |
| `-o <name>` | 是 | 输出路径,可含目录(生成 `<name>.c` / `<name>.h`;符号名取自文件名部分) |
| `-k <1..16>` | 否 | 霍夫曼表训练的簇数量(默认 `1`) |
| `-v` | 否 | 打印每个数组的压缩统计 |
| `-V` | 否 | 打印版本号后退出(也支持 `--version`) |
| `-h` | 否 | 帮助 |

`-k` 用少量表 ROM 换压缩率:所有数组按字节值分布做 k-means 聚类分成 `k`
簇,每簇训练一张霍夫曼表(每张约 30 B ROM),每个压缩块的头字节选择自己
的表(4 bit `cluster_id`)。`-k 1` 全部共用一张表;数据统计特性差异大时
(例如暗场景与亮场景混合),更大的 `k` 更有优势。`k` 会被钳制到不超过
数组总数,聚类初始化是确定性的——同一输入加同一 `-k` 总是生成完全相同
的输出文件。

## 输入格式

解析器扫描 `.c` 文件里所有顶层数组初始化(`= { ... }`),自动识别两种形态:

```c
/* 形态 A - 二维位图数组:每个内层 { } 是一个数组 */
const unsigned char IMG[][1024] = { {0xFF, ...}, {0x00, ...}, ... };

/* 形态 B - 一维数组:唯一的 { } 是一个数组 */
const unsigned char FONT[] = { 0x3C, 0x42, ... };
```

- 值必须是 `0xXX` 十六进制字节(0..255);其他内容一律跳过,超过 `0xFF`
  的值会报错退出。
- 注释、字符串与字符字面量会被跳过;函数体(前面没有 `=` 的
  `{ ... }`)会被忽略。

## 处理流程

```
解析 -> 逐数组 XOR+ZRL(变小才启用) -> k-means 聚类(k 张表)
     -> 霍夫曼编码每个数组 -> 无损回验 -> 写出 .c/.h
```

写入任何文件之前,每个压缩数组都会被重新解码并与原数据逐字节比对;任何
不一致都会中止且不产生输出。(超过 64 KB 的数组跳过该检查并给出警告。)
每个数组是否走 XOR+ZRL 变换记录在各自压缩块的头字节里,同一文件内混用
没有问题。

## 生成文件

`<name>.c` 和 `<name>.h` 成对写入同一目录:`-o` 不含目录时是当前工作
目录,含目录时写进该目录。数据符号**沿用输入数组的原名**(解析器会读取
声明符),只有表和宏从 `-o` 的文件名部分派生,先被清洗成合法的 C 标识符
片段(非 `[A-Za-z0-9_]` 字符变 `_`,数字开头则加 `_` 前缀):

| 输入形态 | 生成的符号 |
|---|---|
| 一维 `FOO[] = {...}` | `const unsigned char FOO[<字节数>]`——压缩块直接用原名 |
| 二维 `FOO[][N] = {...}` | 静态块 `FOO_0..FOO_{n-1}`,外加 `const unsigned char *const FOO[n]`(指针表,原名)和 `const unsigned int FOO_LEN[n]`(基于 sizeof 的长度表) |
| 通用 | `h4b_<name>_tables[]`(霍夫曼表)和 `H4B_<NAME>_CLUSTER_COUNT`(表张数) |

`.h` 里所有 extern 都带尺寸,消费端可以直接用 `sizeof(FOO)` 和
`sizeof(FOO)/sizeof(FOO[0])`。极少数取不到名字的数组会回退成
`h4b_<name>_data_<i>` 并给出警告;若生成的符号重名(例如二维 `FOO` 旁边
还有一个名为 `FOO_0` 的一维数组),写文件之前就会报错中止。

生成的 `.c` 自包含(只包含自己的 `.h`)。单片机侧典型集成:把生成的
`.c` 和 `src/huffman4bit.c` 加入构建,包含路径加上 `src/`,然后:

```c
#include "huffman4bit.h"
#include "mydata.h"

static unsigned char frame[1024];
unsigned int len;

/* 一维输入:压缩块用原名 */
h4b_decode(FONT_8x16, sizeof(FONT_8x16), h4b_mydata_tables, frame, &len);

/* 二维输入:指针表和长度表用原名 */
h4b_decode(IMAGES[i], IMAGES_LEN[i], h4b_mydata_tables, frame, &len);
```

## 输出示例(`-v`)

用上面「用法」里的命令编码随仓库附带的 Bad Apple!! 资源的实际输出:

```
Loaded 3288 array(s) from '../examples/assets/badapple.c'.
Preprocess: 3288/3288 array(s) use XOR+ZRL.
Training (k=2)...
Verified 3288/3288 array(s) lossless.
Wrote badapple_h4b.c and badapple_h4b.h
------------------------------------------------------------
  idx   original   compressed   ratio   clus  prep
    0      1024          6      0.6%     0    xz
    ...                                                  
------------------------------------------------------------
  total  3366912     379014     11.3%    (k=2, tables=60 B)
```

`prep` 列显示该数组经过 XOR+ZRL 变换(`xz`)还是原样存储(`-`);`clus`
列显示压缩块使用的簇表编号。
