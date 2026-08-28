#ifndef __INC_HUFFMAN4BIT_H
#define __INC_HUFFMAN4BIT_H

#if defined (__cplusplus)
extern "C" {
#endif

/* Compile-time switch: decode-only (default, MCU friendly) vs encode+decode.
 * Default is 0 so that MCU projects can #include "huffman4bit.h" and compile
 * huffman4bit.c with zero extra configuration.
 * Encoder/PC tools override this with -DUSE_ENCODE=1 in their CFLAGS. */
#ifndef USE_ENCODE
#define USE_ENCODE          ( 0U )
#endif
#define BIT_CNT             ( 4U )                     /* symbol bit count ("symbol" is the smallest unit of data) */
#define MAX_SYMBOLS         ( 1U << BIT_CNT )          /* possible symbols count */
#define MAX_NODES           ( 2U * MAX_SYMBOLS - 1U )  /* Huffman tree nodes count */
#define MAX_CLUSTER_COUNT   ( 16U )                    /* maximum number of clusters(if equal to 1, not use clustering) */
#define MAX_ITER            ( 100U )                   /* maximum number of clustering iterations */

#if defined(__C51__)
typedef unsigned char  uint8_t;
typedef signed char    int8_t;
typedef unsigned int   uint16_t;
typedef int            int16_t;
typedef unsigned long  uint32_t;
typedef signed long    int32_t;
#else
#include <stdint.h>
#endif

typedef uint8_t huffman_tree_tbl_t[MAX_SYMBOLS - 1][2];
typedef double symbol_frequencies_t[MAX_SYMBOLS];

#if USE_ENCODE
typedef struct
{
    uint32_t freq;
    int8_t parent;
    int8_t left;
    int8_t right;
} huffman_node_t;

typedef struct
{
    uint32_t code;
    uint8_t length;
} huffman_code_t;

typedef struct
{
    huffman_node_t nodes[MAX_NODES];
    huffman_code_t code_table[MAX_SYMBOLS];
} huffman_tree_t;


typedef struct
{
    symbol_frequencies_t centroid_points[MAX_CLUSTER_COUNT];
    uint32_t point_index[MAX_CLUSTER_COUNT];
    uint8_t points_count;
} centroid_t;

typedef struct
{
    uint32_t array_size;
    uint8_t *array_ptr;
    symbol_frequencies_t feature;
    uint8_t cluster_id;
} array_data_t;


typedef struct
{
    array_data_t *array_data;
    uint32_t array_count;
    uint32_t capacity;
} input_buffer_t;



void h4b_init(void);
void h4b_encode(uint8_t *input_data, uint32_t input_len, uint8_t transform,
                uint8_t *output_data, uint32_t *output_len);
void h4b_add_array(uint8_t *input_data, uint32_t input_len);
void h4b_clustering(uint8_t k);
void h4b_create_huffman_tree(huffman_tree_tbl_t *tables);

/* Per-array preprocessing: XOR with previous byte, then zero-run-length
 * coding ("00 00 cnt" = cnt+2 zeros). Applied before Huffman coding when it
 * shrinks the array; flagged in the stream header byte.
 * output_data must have room for 2*input_len bytes. Returns output length. */
uint32_t h4b_xor_zrl_encode(const uint8_t *input_data, uint32_t input_len,
                                uint8_t *output_data);
#endif /* !USE_ENCODE */

/* Byte-sink callback for the streaming decoder (MCU: wire it to the display
 * bus to decode straight to the screen with no frame buffer). */
typedef void (*h4b_emit_fn)(void *ctx, uint8_t byte);

void h4b_decode(uint8_t *input_data, uint32_t input_len, huffman_tree_tbl_t *table, uint8_t *output_data, uint32_t *output_len);
void h4b_decode_stream(const uint8_t *input_data, uint32_t input_len,
                       const huffman_tree_tbl_t *table,
                       h4b_emit_fn emit, void *ctx);

#if defined (__cplusplus)
}   /* extern "C" */
#endif


#endif /* !__HUFFMAN4BIT_H__ */
