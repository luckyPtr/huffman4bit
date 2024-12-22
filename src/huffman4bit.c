#include "huffman4bit.h"
#include "stdio.h"
#include <stdint.h>
#include <string.h>

#if USE_ENCODE
#include "math.h"
/**********************
 *  STATIC VARIABLES
 **********************/
static huffman_tree_t huffman_tree;
static centroid_t centroids;    // 簇中心
/**********************
 *  STATIC PROTOTYPES
 **********************/
static void cal_huffman_table(void);
static void export_huffman_tree_table(huffman_tree_tbl_t *table);
static void build_huffman_tree(void);
static void build_huffman_tree(void);
static void set_bit(h4b_uint8_t *output_data, h4b_uint32_t pos, h4b_uint8_t bit);

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void cal_huffman_table(void)
{
    h4b_uint8_t i;
    h4b_uint8_t index;
    h4b_uint8_t parent;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        index = i;
        while (huffman_tree.nodes[index].parent != -1)
        {
            parent = (h4b_uint8_t)huffman_tree.nodes[index].parent;
            if (huffman_tree.nodes[parent].right == index)
            {
                huffman_tree.code_table[i].code |= ((h4b_uint32_t)1 << huffman_tree.code_table[i].length);
            }
            huffman_tree.code_table[i].length++;
            index = parent;
        }
    }
}

// 导出Huffman tree表 
// 仅保留非叶子节点
static void export_huffman_tree_table(huffman_tree_tbl_t *table)
{
    h4b_uint8_t i;
    for (i = MAX_SYMBOLS; i < MAX_NODES; i++)
    {
        (*table)[i - MAX_SYMBOLS][0] = (h4b_uint8_t)huffman_tree.nodes[i].left;
        (*table)[i - MAX_SYMBOLS][1] = (h4b_uint8_t)huffman_tree.nodes[i].right;
    }
}

// 构建Huffman tree
static void build_huffman_tree(void)
{
    h4b_int8_t node_count = MAX_SYMBOLS;
    h4b_int8_t min1, min2;
    h4b_int8_t i;
    while (node_count < MAX_NODES)
    {
        min1 = min2 = -1;
        // 找到频率最小的两个节点
        for (i = 0; i < node_count; i++) 
        {
            if (huffman_tree.nodes[i].parent == -1) 
            {
                if (min1 == -1 || huffman_tree.nodes[i].freq < huffman_tree.nodes[min1].freq) 
                {
                    min2 = min1;
                    min1 = i;
                } 
                else if (min2 == -1 || huffman_tree.nodes[i].freq < huffman_tree.nodes[min2].freq) 
                {
                    min2 = i;
                }
            }
        }

        // 合并两个节点
        huffman_tree.nodes[node_count].freq = huffman_tree.nodes[min1].freq + huffman_tree.nodes[min2].freq;
        huffman_tree.nodes[node_count].left = min1;
        huffman_tree.nodes[node_count].right = min2;
        huffman_tree.nodes[node_count].parent = -1;

        huffman_tree.nodes[min1].parent = node_count;
        huffman_tree.nodes[min2].parent = node_count;

        node_count++;
    }

    for (int i = 0; i < MAX_SYMBOLS; i++)
    {
        printf("%02d %f\n", i, (double)huffman_tree.nodes[i].freq / huffman_tree.nodes[30].freq);
    }
}

static void set_bit(h4b_uint8_t *output_data, h4b_uint32_t pos, h4b_uint8_t bit)
{
    if (bit)
    {
        output_data[pos >> 3] |= (1 << (7 - (pos & 0x07)));
    }
    else
    {
        output_data[pos >> 3] &= ~(1 << (7 - (pos & 0x07)));
    }
}

static h4b_float euclidean_distance(data_point_t p1, data_point_t p2)
{
    h4b_uint8_t i;
    h4b_float sum = 0;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        sum += (p1.percent[i] - p2.percent[i]) * (p1.percent[i] - p2.percent[i]);
    }
    return sqrt(sum);
}

void h4b_add_centroid(h4b_uint8_t *input_data, h4b_uint32_t input_len)
{
    data_percent_t percent;
    h4b_uint32_t sum[MAX_SYMBOLS] = {0};
    h4b_uint32_t i;
    h4b_uint8_t h, l;
    for (i = 0; i < input_len; i++)
    {
        h = input_data[i] >> 4;
        l = input_data[i] & 0x0F;
        sum[h]++;
        sum[l]++;
    }
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        percent[i] = (h4b_float)sum[i] / (input_len * 2);
    }

    memcpy(centroids.centroid_points[centroids.points_count], percent, MAX_SYMBOLS);
    centroids.points_count++;
}

// 更新簇的质心
void update_centroids(data_point_t* centroids, h4b_uint8_t k, data_point_t *points, h4b_uint32_t point_count) {
    h4b_uint32_t i, j, d;

    for (i = 0; i < k; i++) 
    {
        double sum[MAX_SYMBOLS] = {0};
        uint32_t count = 0;

        // 累加属于当前簇的所有点
        for (j = 0; j < point_count; j++) {
            if (points[j].cluster_id == i) {
                for (d = 0; d < MAX_SYMBOLS; d++) {
                    sum[d] += points[j].percent[d];
                }
                count++;
            }
        }

        // 更新质心坐标
        if (count > 0) 
        {
            for (d = 0; d < MAX_SYMBOLS; d++) {
                centroids[i].percent[d] = sum[d] / count;
            }
        }
    }
}

void k_means(data_point_t *points, h4b_uint32_t point_count, data_point_t *centroids, h4b_uint8_t k)
{
    int iter = 0;
    int changed;

    do {
        changed = 0;

        // 为每个点分配到距离最近的质心
        for (int i = 0; i < point_count; i++) {
            double min_dist = euclidean_distance(points[i], centroids[0]);
            int min_cluster = 0;

            for (int j = 1; j < k; j++) {
                double dist = euclidean_distance(points[i], centroids[j]);
                if (dist < min_dist) {
                    min_dist = dist;
                    min_cluster = j;
                }
            }

            // 如果点的簇发生变化，更新簇标识
            if (points[i].cluster_id != min_cluster) {
                points[i].cluster_id = min_cluster;
                changed = 1;
            }
        }

        // 更新质心
        update_centroids(centroids, points, point_count, k);

        iter++;
    } while (changed && iter < MAX_ITER);    
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void h4b_reset_tree(void)
{
    uint8_t i;
    for (i = 0; i < MAX_NODES; i++)
    {
        huffman_tree.nodes[i].freq = 0;
        huffman_tree.nodes[i].parent = huffman_tree.nodes[i].left = huffman_tree.nodes[i].right = -1;
    }
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        huffman_tree.code_table[i].code = 0;
        huffman_tree.code_table[i].length = 0;
    }

    centroids.points_count = 0;
}

void h4b_count_freq_append(h4b_uint8_t *data, h4b_uint32_t size)
{
    uint8_t h, l;
    uint32_t i;
    
    for (i = 0; i < size; i++)
    {
        h = (data[i] >> 4) & 0x0F;
        l = data[i] & 0x0F;
        huffman_tree.nodes[h].freq++;
        huffman_tree.nodes[l].freq++;
    }
}


void h4b_create_huffman_tree(huffman_tree_tbl_t *table)
{
    build_huffman_tree();
    cal_huffman_table();
    export_huffman_tree_table(table);
}



void h4b_encode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_int8_t j;
    h4b_uint32_t bit_pos = 8;
    h4b_uint32_t code;
    h4b_uint8_t length;
    h4b_uint8_t data_h, data_l;
    h4b_uint8_t padding;

    for (i = 0; i < input_len; i++)
    {
        data_h = input_data[i] >> 4;
        data_l = input_data[i] & 0x0F;

        code = huffman_tree.code_table[data_h].code;
        length = huffman_tree.code_table[data_h].length;
        for (j = length - 1; j >= 0; j--, bit_pos++)
        {
            set_bit(output_data, bit_pos, (code >> j) & 0x01);
        }

        code = huffman_tree.code_table[data_l].code;
        length = huffman_tree.code_table[data_l].length;
        for (j = length - 1; j >= 0; j--, bit_pos++)
        {
            set_bit(output_data, bit_pos, (code >> j) & 0x01);
        }
    }

    padding = (8 - (bit_pos & 0x07)) & 0x07;
    for (i = 0; i < padding; i++, bit_pos++)
    {
        set_bit(output_data, bit_pos, 0);
    }
    output_data[0] = padding;

    *output_len =  bit_pos / 8;
}


#endif // !USE_ENCODE


void h4b_decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, huffman_tree_tbl_t *table, h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_uint8_t byte;
    h4b_uint8_t index = MAX_NODES - MAX_SYMBOLS - 1;
    h4b_uint32_t byte_pos = 0;
    h4b_uint8_t high_bits = 1;
    h4b_uint8_t padding = input_data[0] & 0x07;
    h4b_uint8_t *data_ptr = &input_data[1];   // 第一个字节是头跳过
    h4b_uint32_t total_bits = ((input_len - 1) << 3) - padding;
    huffman_tree_tbl_t *current_table = &table[input_data[0] >> 3];

    for (i = 0; i < total_bits; i++)
    {
        byte = data_ptr[i >> 3];
        if ((byte << (i & 0x07)) & 0x80)
        {
            index = (*current_table)[index][1];
        }
        else
        {
            index = (*current_table)[index][0];
        }

        if (index < MAX_SYMBOLS)
        {
            if (high_bits)
            {
                output_data[byte_pos] = index << 4;
            }
            else
            {
                output_data[byte_pos++] |= index;
            }
            index = MAX_NODES - MAX_SYMBOLS - 1;
            high_bits ^= 1;
        }
        else
        {
            index -= MAX_SYMBOLS;
        }
    }

    *output_len = byte_pos;
}