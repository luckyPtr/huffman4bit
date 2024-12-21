#ifndef __HUFFMAN4BIT_H__
#define __HUFFMAN4BIT_H__

#define MAX_CNT     64
#define BIT_CNT         4
#define MAX_SYMBOLS     (1U << BIT_CNT)
#define MAX_NODES       (2 * MAX_SYMBOLS - 1)

typedef unsigned char h4b_uint8_t;
typedef char h4b_int8_t;
typedef unsigned short int h4b_uint16_t;
typedef unsigned int h4b_uint32_t;

#define H4B_CONST   const

typedef struct 
{
    h4b_uint32_t freq;
    h4b_int8_t parent;
    h4b_int8_t left;
    h4b_int8_t right;
} huffman_node_t;

typedef struct
{
    h4b_uint8_t code;
    h4b_uint8_t length;
} huffman_code_t;

typedef struct 
{
    huffman_node_t nodes[MAX_NODES];
    huffman_code_t code_table[MAX_SYMBOLS];
} huffman_tree_t;





void h4b_compress(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len, h4b_uint8_t *huffman_tree);
void decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t (*huffman_tree)[2], h4b_uint8_t *output_data, h4b_uint32_t *output_len);

#endif // !__HUFFMAN4BIT_H__
