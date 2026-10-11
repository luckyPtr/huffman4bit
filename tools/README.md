# tools — h4b_encoder

English | [简体中文](README.zh-CN.md)

The PC-side encoder for huffman4bit: feed it a C source file containing hex
byte arrays (OLED bitmaps, font data, ...), and it produces a compressed
`<name>.c` / `<name>.h` pair that the decode-only library consumes directly
on the MCU. 

## Build

Requires a C compiler (gcc / clang / mingw) and `make`. All build products
land in `build/`:

```bash
make          # -> build/h4b_encoder(.exe)
make clean    # remove build/
```

## Usage

```bash
./build/h4b_encoder -i ../examples/assets/badapple.c -o badapple_h4b -k 2 -v
# writes ./badapple_h4b.c and ./badapple_h4b.h into the CURRENT directory
```

| Option | Required | Description |
|---|---|---|
| `-i <file>` | yes | Input C source containing `0xXX` hex arrays |
| `-o <name>` | yes | Output path, may include a directory (produces `<name>.c` / `<name>.h`; symbols derive from the file-name part) |
| `-k <1..16>` | no  | Cluster count for Huffman table training (default `1`) |
| `-v`         | no  | Print per-array compression statistics |
| `-V`         | no  | Print version and exit (also `--version`) |
| `-h`         | no  | Help |

`-k` trades a little table ROM for ratio: the arrays are grouped into `k`
clusters by k-means (over their byte-value distributions), one Huffman table
is trained per cluster (~30 B of ROM each), and every compressed blob's
header byte selects its table (4-bit `cluster_id`). `-k 1` shares a single
table; larger `k` helps when the data mixes very different statistics.
`k` is clamped to the number of input arrays, and clustering is seeded
deterministically, so the same input and `-k` always produce byte-identical
output.

## Input format

The parser scans any `.c` file for top-level array initializers (`= { ... }`)
and auto-detects two shapes:

```c
/* Shape A - 2D bitmap array: each inner { } becomes one array */
const unsigned char IMG[][1024] = { {0xFF, ...}, {0x00, ...}, ... };

/* Shape B - 1D array: the single { } becomes one array */
const unsigned char FONT[] = { 0x3C, 0x42, ... };
```

- Values must be `0xXX` hex bytes (0..255); anything else is skipped, values
  above `0xFF` abort with an error.
- Comments, string and char literals are skipped; function bodies
  (`{ ... }` not preceded by `=`) are ignored.

## What it does

```
parse -> per-array XOR+ZRL (when it shrinks) -> k-means clustering (k tables)
      -> Huffman-encode every array -> decode-verify losslessly -> write .c/.h
```

Before anything is written, every compressed array is decoded again and
compared with its original; any mismatch aborts the run with no output
written. (Arrays larger than 64 KB skip this check with a warning.) The
per-array XOR+ZRL decision is recorded in each blob's header byte, so mixed
use within one file is fine.

## Generated files

`<name>.c` and `<name>.h` are written as a pair into the same directory:
the current working directory by default, or the directory part of `-o`
when it has one. Data symbols keep the **original array names** (the parser
reads the declarator); only the tables and macros derive from the file-name
part of `-o`, which is sanitised into a valid C identifier fragment
(non `[A-Za-z0-9_]` becomes `_`, a leading digit gets a `_` prefix):

| Input shape | Emitted symbols |
|---|---|
| 1D `FOO[] = {...}` | `const unsigned char FOO[<bytes>]` — the compressed blob under the original name |
| 2D `FOO[][N] = {...}` | static blobs `FOO_0..FOO_{n-1}` plus `const unsigned char *const FOO[n]` (pointer table, original name) and `const unsigned int FOO_LEN[n]` (sizeof-based lengths) |
| any | `h4b_<name>_tables[]` (Huffman tables) and `H4B_<NAME>_CLUSTER_COUNT` (table count) |

All externs in the `.h` are sized, so consumers can use `sizeof(FOO)` and
`sizeof(FOO)/sizeof(FOO[0])` directly. A rare nameless input array falls
back to `h4b_<name>_data_<i>` with a warning; duplicate emitted symbols
(e.g. a 1D array named `FOO_0` next to a 2D `FOO`) abort the run before
anything is written.

The generated `.c` is self-contained (it only includes its own `.h`).
Typical MCU integration: add the generated `.c` plus `src/huffman4bit.c` to
the build, include the `.h` with `src/` on the include path, then:

```c
#include "huffman4bit.h"
#include "mydata.h"

static unsigned char frame[1024];
unsigned int len;

/* 1D input: the blob keeps the original name */
h4b_decode(FONT_8x16, sizeof(FONT_8x16), h4b_mydata_tables, frame, &len);

/* 2D input: pointer + length tables under the original name */
h4b_decode(IMAGES[i], IMAGES_LEN[i], h4b_mydata_tables, frame, &len);
```

## Example (`-v`)

Transcript from encoding the shipped Bad Apple!! asset with the Usage
command above:

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

`prep` shows whether the array went through the XOR+ZRL transform (`xz`) or
was stored raw (`-`); `clus` shows which cluster's table the blob uses.
