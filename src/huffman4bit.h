#ifndef __HUFFMAN4BIT_H__
#define __HUFFMAN4BIT_H__

#define USE_ENCODE      (1)     // 是否使用编码功能 默认关闭(编码功能使用会占用大量的RAM，如果仅使用解码功能可不开启编码)
#define BIT_CNT         (4U)
#define MAX_SYMBOLS     (1U << BIT_CNT)
#define MAX_NODES       (2U * MAX_SYMBOLS - 1)
#define MAX_POINTS_CNT  (8)    // K-Means算法最多可以设置的中心点数量(设置为1即不使用K-Means算法)
#define MAX_ITER        (100)

typedef unsigned char h4b_uint8_t;
typedef char h4b_int8_t;
typedef unsigned short int h4b_uint16_t;
typedef unsigned int h4b_uint32_t;
typedef int h4b_int32_t;
typedef double h4b_float;
typedef h4b_uint8_t huffman_tree_tbl_t[MAX_SYMBOLS - 1][2];
typedef h4b_float data_percent_t[MAX_SYMBOLS];

#if USE_ENCODE
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

typedef struct
{
    h4b_float percent[MAX_SYMBOLS];
    h4b_uint8_t cluster_id;
} data_point_t;

typedef struct
{
    data_percent_t centroid_points[MAX_POINTS_CNT];
    h4b_uint8_t points_count;
    h4b_uint8_t changed;
} centroid_t;




void h4b_reset_tree(void);
void h4b_count_freq_append(h4b_uint8_t *data, h4b_uint32_t size);
void h4b_create_huffman_tree(huffman_tree_tbl_t *table);
void h4b_encode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len);
void h4b_add_centroid(h4b_uint8_t *input_data, h4b_uint32_t input_len);
#endif // !USE_ENCODE
void h4b_decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, huffman_tree_tbl_t *table, h4b_uint8_t *output_data, h4b_uint32_t *output_len);

#endif // !__HUFFMAN4BIT_H__
