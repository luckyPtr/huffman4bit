#ifndef __HUFFMAN4BIT_H__
#define __HUFFMAN4BIT_H__

#define MAX_CNT         64U
#define BIT_CNT         4U
#define MAX_SYMBOLS     (1U << BIT_CNT)
#define MAX_NODES       (2U * MAX_SYMBOLS - 1)

typedef unsigned char h4b_uint8_t;
typedef char h4b_int8_t;
typedef unsigned short int h4b_uint16_t;
typedef unsigned int h4b_uint32_t;
typedef int h4b_int32_t;

typedef h4b_uint8_t huffman_tree_tbl_t[MAX_SYMBOLS - 1][2];


typedef struct 
{
    h4b_uint32_t freq;
    h4b_int8_t parent;
    h4b_int8_t left;
    h4b_int8_t right;
} huffman_node_t;

typedef struct
{
    h4b_uint32_t code;
    h4b_uint8_t length;
} huffman_code_t;

typedef struct 
{
    huffman_node_t nodes[MAX_NODES];
    huffman_code_t code_table[MAX_SYMBOLS];
} huffman_tree_t;




void h4b_reset_tree(void);
void h4b_count_freq_append(h4b_uint8_t *data, h4b_uint32_t size);
void h4b_create_huffman_tree(huffman_tree_tbl_t *table);
void h4b_encode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len);
void h4b_decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, huffman_tree_tbl_t *table, h4b_uint8_t *output_data, h4b_uint32_t *output_len);

#endif // !__HUFFMAN4BIT_H__
