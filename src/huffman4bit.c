/**
 * Huffman4bit algorithm implementation.
 */

#include "huffman4bit.h"

#if USE_ENCODE
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
/**********************
 *  STATIC VARIABLES
 **********************/
static huffman_tree_t huffman_tree[MAX_CLUSTER_COUNT];     // Huffman tree for each cluster
static centroid_t centroids;
static input_buffer_t input_buffer;

/********************** 
 *  STATIC PROTOTYPES
 **********************/
static void cal_huffman_table(h4b_uint8_t k);
static void export_huffman_tree_table(h4b_uint8_t k, huffman_tree_tbl_t *table);
static void build_huffman_tree(h4b_uint8_t k);
static h4b_float euclidean_distance(symbol_frequencies_t p1, symbol_frequencies_t p2);
static void count_freq_append(h4b_uint8_t k, h4b_uint8_t *data, h4b_uint32_t size);
static void assign_points(void);
static void update_centroids(void);
static void calculate_features(void);
static void init_centroids(h4b_uint8_t k);
static void k_means(void);
static h4b_uint8_t find_nearest_cluster(h4b_uint8_t *input_data, h4b_uint32_t input_len);
static void set_bit(h4b_uint8_t *output_data, h4b_uint32_t pos, h4b_uint8_t bit);

/**********************
 *   STATIC FUNCTIONS
 **********************/
/**
 * @brief Calculate the Huffman codes for each symbol in the input data.
 * 
 * @param k Cluster index.
 */
static void cal_huffman_table(h4b_uint8_t k)
{
    h4b_uint8_t i;
    h4b_uint8_t index;
    h4b_uint8_t parent;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        index = i;
        while (huffman_tree[k].nodes[index].parent != -1)
        {
            parent = (h4b_uint8_t)huffman_tree[k].nodes[index].parent;
            if (huffman_tree[k].nodes[parent].right == index)
            {
                huffman_tree[k].code_table[i].code |= ((h4b_uint32_t)1 << huffman_tree[k].code_table[i].length);
            }
            huffman_tree[k].code_table[i].length++;
            index = parent;
        }
    }
}

/**
 * @brief Export the Huffman tree table.
 * 
 * @param k Cluster index.
 * @param table Pointer to the Huffman tree table.
 * @note Only export nodes that are not leaf nodes.
 *       The table is a 2D array where the first dimension represents the nodes and the second dimension represents the left and right children of each node.   
 */
static void export_huffman_tree_table(h4b_uint8_t k, huffman_tree_tbl_t *table)
{
    h4b_uint8_t i;
    for (i = MAX_SYMBOLS; i < MAX_NODES; i++)
    {
        (*table)[i - MAX_SYMBOLS][0] = (h4b_uint8_t)huffman_tree[k].nodes[i].left;
        (*table)[i - MAX_SYMBOLS][1] = (h4b_uint8_t)huffman_tree[k].nodes[i].right;
    }
}

/**
 * @brief Build the Huffman tree for the input data.
 * 
 * @param k Cluster index.
 */
static void build_huffman_tree(h4b_uint8_t k)
{
    h4b_int8_t node_count = MAX_SYMBOLS;
    h4b_int8_t min1, min2;
    h4b_int8_t i;
    
    while ((h4b_uint8_t)node_count < MAX_NODES)
    {
        min1 = min2 = -1;
        // find the two nodes with the smallest frequency
        for (i = 0; i < node_count; i++) 
        {
            if (huffman_tree[k].nodes[(h4b_uint8_t)i].parent == -1) 
            {
                if (min1 == -1 || huffman_tree[k].nodes[(h4b_uint8_t)i].freq < huffman_tree[k].nodes[(h4b_uint8_t)min1].freq) 
                {
                    min2 = min1;
                    min1 = i;
                } 
                else if (min2 == -1 || huffman_tree[k].nodes[(h4b_uint8_t)i].freq < huffman_tree[k].nodes[(h4b_uint8_t)min2].freq) 
                {
                    min2 = i;
                }
            }
        }

        // merge the two nodes
        huffman_tree[k].nodes[(h4b_uint8_t)node_count].freq = huffman_tree[k].nodes[(h4b_uint8_t)min1].freq + huffman_tree[k].nodes[(h4b_uint8_t)min2].freq;
        huffman_tree[k].nodes[(h4b_uint8_t)node_count].left = min1;
        huffman_tree[k].nodes[(h4b_uint8_t)node_count].right = min2;
        huffman_tree[k].nodes[(h4b_uint8_t)node_count].parent = -1;

        huffman_tree[k].nodes[(h4b_uint8_t)min1].parent = node_count;
        huffman_tree[k].nodes[(h4b_uint8_t)min2].parent = node_count;

        node_count++;
    }
}


/**
 * @brief Calculate the Euclidean distance between two data points.
 * 
 * @param p1 Pointer to the first data point.
 * @param p2 Pointer to the second data point.
 * @return h4b_float Euclidean distance between the two data points.
 */
static h4b_float euclidean_distance(symbol_frequencies_t p1, symbol_frequencies_t p2)
{
    h4b_uint8_t i;
    h4b_float sum = 0;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        sum += (p1[i] - p2[i]) * (p1[i] - p2[i]);
    }
    return sqrt(sum);
}


/**
 * @brief Count the frequency of each symbol in the input data and append it to the Huffman tree.
 * 
 * @param k Cluster index.
 * @param data Pointer to the input data array.
 * @param size Length of the input data.
 */
static void count_freq_append(h4b_uint8_t k, h4b_uint8_t *data, h4b_uint32_t size)
{
    h4b_uint8_t h, l;
    h4b_uint32_t i;
    
    for (i = 0; i < size; i++)
    {
        h = (data[i] >> 4) & 0x0F;
        l = data[i] & 0x0F;
        huffman_tree[k].nodes[h].freq++;
        huffman_tree[k].nodes[l].freq++;
    }
}


/**
 * @brief Assign points to the nearest cluster.
 */
static void assign_points(void) {
    h4b_uint32_t i;
    h4b_uint8_t j;
    h4b_float min_dist, dist;
    h4b_uint8_t min_cluster;

    for (i = 0; i < input_buffer.array_count; i++) {
        min_dist = euclidean_distance(input_buffer.array_data[i].feature, centroids.centroid_points[0]);
        min_cluster = 0;

        for (j = 1; j < centroids.points_count; j++) {
            dist = euclidean_distance(input_buffer.array_data[i].feature, centroids.centroid_points[j]);
            if (dist < min_dist) {
                min_dist = dist;
                min_cluster = j;
            }
        }

        input_buffer.array_data[i].cluster_id = min_cluster;
    }
}


/**
 * @brief Update the centroids.
 */
static void update_centroids(void)
{
    h4b_uint32_t i, j;
    h4b_uint32_t cluster_size[MAX_CLUSTER_COUNT] = {0};
    symbol_frequencies_t sum[MAX_CLUSTER_COUNT] = {0};

    // 计算每个簇的大小和特征值之和
    for (i = 0; i < input_buffer.array_count; i++)
    {
        cluster_size[input_buffer.array_data[i].cluster_id]++;
        for (j = 0; j < MAX_SYMBOLS; j++)
        {
            sum[input_buffer.array_data[i].cluster_id][j] += input_buffer.array_data[i].feature[j];
        }
    }

    // 更新质心
    for (i = 0; i < centroids.points_count; i++)
    {
        for (j = 0; j < MAX_SYMBOLS; j++)
        {
            centroids.centroid_points[i][j] = sum[i][j] / cluster_size[i];
        }
    }
}


/**
 * @brief Calculate the features of the input data.
 * 
 * The features are calculated as the ratio of the number of occurrences of each symbol to the total number of symbols in the array.
 */
static void calculate_features(void)
{
    h4b_uint32_t i, j;
    h4b_uint32_t data_count[MAX_SYMBOLS];
    h4b_uint32_t total_count;
    h4b_uint8_t h, l;
    array_data_t *array = NULL;

    for (i = 0; i < input_buffer.array_count; i++)
    {
        memset(data_count, 0, sizeof(data_count));
        total_count = 0;
        array = &input_buffer.array_data[i];
        for (j = 0; j < array->array_size; j++)
        {
            h = array->array_ptr[j] >> 4;
            l = array->array_ptr[j] & 0x0F;
            data_count[h]++;
            data_count[l]++;
            total_count += 2;
        }

        for (j = 0; j < MAX_SYMBOLS; j++)
        {
            array->feature[j] = (h4b_float)data_count[j] / total_count;
        }

    }
}

/**
 * @brief Initialize the centroids.
 * 
 * @param k Number of centroids.
 */
void init_centroids(h4b_uint8_t k)
{
    h4b_uint8_t i, j;

    srand((h4b_uint32_t)time(NULL));
    
    if (k > MAX_CLUSTER_COUNT)
    {
        k = MAX_CLUSTER_COUNT;
    }

    for (i = 0; i < k;)
    {
        centroids.point_index[i] = (h4b_uint32_t)((h4b_uint32_t)rand() % input_buffer.array_count);
        for (j = 0; j <= i; j++)
        {
            if (i == j)
            {
                i++;
                break;
            }
            if (centroids.point_index[i] == centroids.point_index[j])
            {
                break;
            }
        }
    }

    for (i = 0; i < k; i++)
    {
        memcpy(centroids.centroid_points[i], input_buffer.array_data[centroids.point_index[i]].feature, sizeof(symbol_frequencies_t));
    }
    centroids.points_count = k;
}

/**
 * @brief Perform K-means clustering on the input data.
 */
static void k_means(void)
{
    h4b_uint32_t iter = 0;
    h4b_uint8_t changed;
    symbol_frequencies_t old_centroids[MAX_CLUSTER_COUNT];

    do {
        memcpy(old_centroids, centroids.centroid_points, sizeof(centroids.centroid_points));

        assign_points();
        update_centroids();

        changed = 0;
        for (int i = 0; i < centroids.points_count; i++) {
            if (euclidean_distance(old_centroids[i], centroids.centroid_points[i]) > 1e-6) {
                changed = 1;
                break;
            }
        }

        iter++;
    } while (changed && iter < MAX_ITER);
}


/**
 * @brief Find the nearest cluster for the input data.
 * 
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data.
 * @return h4b_uint8_t Nearest cluster index.
 */
static h4b_uint8_t find_nearest_cluster(h4b_uint8_t *input_data, h4b_uint32_t input_len)
{
    h4b_uint32_t i;
    h4b_uint8_t h, l;
    h4b_uint32_t data_count[MAX_SYMBOLS];
    h4b_uint32_t total_count;
    symbol_frequencies_t feature;
    h4b_float min_dist, dist;
    h4b_uint8_t min_cluster;

    memset(data_count, 0, sizeof(data_count));
    total_count = 0;
    for (i = 0; i < input_len; i++)
    {
        h = input_data[i] >> 4;
        l = input_data[i] & 0x0F;
        data_count[h]++;
        data_count[l]++;
        total_count += 2;
    }

    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        feature[i] = (h4b_float)data_count[i] / total_count;
    }

    min_dist = euclidean_distance(feature, centroids.centroid_points[0]);
    min_cluster = 0;

    for (i = 1; i < centroids.points_count; i++)
    {
        dist = euclidean_distance(feature, centroids.centroid_points[i]);
        if (dist < min_dist)
        {
            min_dist = dist;
            min_cluster = (h4b_uint8_t)i;
        }
    }

    return min_cluster;
}



/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief Initialize the Huffman4bit algorithm.
 */
void h4b_init(void)
{
    h4b_uint8_t i, k;
    for (k = 0; k < MAX_CLUSTER_COUNT; k++)
    {
        for (i = 0; i < MAX_NODES; i++)
        {
            huffman_tree[k].nodes[i].freq = 0;
            huffman_tree[k].nodes[i].parent = huffman_tree[k].nodes[i].left = huffman_tree[k].nodes[i].right = -1;
        }
        for (i = 0; i < MAX_SYMBOLS; i++)
        {
            huffman_tree[k].code_table[i].code = 0;
            huffman_tree[k].code_table[i].length = 0;
        }
    }
    centroids.points_count = 0;

    // 释放原始数组
    if (input_buffer.array_data != NULL)
    {
        for (i = 0; i < input_buffer.array_count; i++)
        {
            free(input_buffer.array_data[i].array_ptr);
        }
        free(input_buffer.array_data);
    }
    input_buffer.array_count = 0;
    input_buffer.capacity = 0;
}


/**
 * @brief Add an array to the input data.
 * 
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data.
 */
void h4b_add_array(h4b_uint8_t *input_data, h4b_uint32_t input_len)
{
    array_data_t *new_array = NULL;

    if (input_buffer.array_count >= input_buffer.capacity)
    {
        if (input_buffer.capacity == 0)
        {
            input_buffer.capacity = 1024;
            input_buffer.array_data = (array_data_t*)malloc(sizeof(array_data_t) * input_buffer.capacity);
        }
        else
        {
            input_buffer.capacity *= 2;
            input_buffer.array_data = (array_data_t*)realloc(input_buffer.array_data, sizeof(array_data_t) * input_buffer.capacity);
        }
    }

    new_array = &input_buffer.array_data[input_buffer.array_count];
    new_array->array_size = input_len;
    new_array->array_ptr = (h4b_uint8_t*)malloc(input_len);
    memcpy(new_array->array_ptr, input_data, input_len);
    input_buffer.array_count++;
}



/**
 * @brief Perform clustering on the input data.
 * 
 * @param k number of centroids.
 */
void h4b_clustering(h4b_uint8_t k)
{
    calculate_features();   // 计算特征
    init_centroids(k);      // 随机初始化质心
    k_means();              // K-means算法
}

/**
 * @brief Create Huffman tree for each cluster.
 * 
 * This function iterates over each cluster and constructs a Huffman tree based on the frequency of symbols in the cluster.
 * It then calculates the Huffman codes and exports the Huffman tree table.
 * 
 * @param tables Pointer to the Huffman tree table array where the Huffman tree tables for each cluster will be stored.
*                Each table is a 2D array where the first dimension represents the nodes and the second dimension represents
*                the left and right children of each node.
 */
void h4b_create_huffman_tree(huffman_tree_tbl_t *tables)
{
    h4b_uint8_t i;
    h4b_uint32_t j;

    for (i = 0; i < centroids.points_count; i++)    // create huffman tree for each cluster
    {
        for (j = 0; j < input_buffer.array_count; j++)
        {
            if (input_buffer.array_data[j].cluster_id == i)
            {
                count_freq_append(i, input_buffer.array_data[j].array_ptr, input_buffer.array_data[j].array_size);
            }
        }
        build_huffman_tree(i);
        cal_huffman_table(i);
        export_huffman_tree_table(i, &tables[i]);
    }	
}


/**
 * @brief Set a specific bit in the output data.
 * 
 * @param output_data Pointer to the output data array.
 * @param pos Position of the bit to set.
 * @param bit Value of the bit to set (0 or 1).
 */
static void set_bit(h4b_uint8_t *output_data, h4b_uint32_t pos, h4b_uint8_t bit)
{
    if (bit)
    {
        output_data[pos >> 3] |= (h4b_uint8_t)(1 << (7 - (pos & 0x07)));
    }
    else
    {
        output_data[pos >> 3] &= (h4b_uint8_t)~(1 << (7 - (pos & 0x07)));
    }
}

/**
 * @brief Encode the input data using the nearest Huffman tree.
 * 
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data.
 * @param output_data Pointer to the output data array.
 * @param output_len Pointer to the length of the output data.
 */
void h4b_encode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_int32_t j;
    h4b_uint32_t bit_pos = 8;
    h4b_uint32_t code;
    h4b_uint8_t length;
    h4b_uint8_t padding;
    h4b_uint8_t cluster;
    h4b_uint8_t data_h, data_l;
    cluster = find_nearest_cluster(input_data, input_len);

    for (i = 0; i < input_len; i++)
    {
        data_h = input_data[i] >> 4;
        data_l = input_data[i] & 0x0F;

        code = huffman_tree[cluster].code_table[data_h].code;
        length = huffman_tree[cluster].code_table[data_h].length;
        for (j = length - 1; j >= 0; j--, bit_pos++)
        {
            set_bit(output_data, bit_pos, (code >> j) & 0x01);
        }

        code = huffman_tree[cluster].code_table[data_l].code;
        length = huffman_tree[cluster].code_table[data_l].length;
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
    output_data[0] = (h4b_uint8_t)((cluster << 3) + padding);

    *output_len =  bit_pos / 8;
}


#endif // !USE_ENCODE


/**
 * @brief Decode the input data using the Huffman tree table.
 * 
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data.
 * @param table Pointer to the Huffman tree table.
 * @param output_data Pointer to the output data array.
 * @param output_len Pointer to the length of the output data.
 */
void h4b_decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, huffman_tree_tbl_t *table, h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_uint8_t byte;
    h4b_uint8_t index = MAX_NODES - MAX_SYMBOLS - 1;
    h4b_uint32_t byte_pos = 0;
    h4b_uint8_t high_bits = 1;
    h4b_uint8_t padding = input_data[0] & 0x07;
    h4b_uint8_t *data_ptr = &input_data[1];   // skip the first byte(custer and padding)
    h4b_uint32_t total_bits = ((input_len - 1) << 3) - padding;
    huffman_tree_tbl_t *current_table = &table[input_data[0] >> 3];

    for (i = 0; i < total_bits; i++)
    {
        byte = data_ptr[i >> 3];
        index = ((byte << (i & 0x07)) & 0x80) ? (*current_table)[index][1] : (*current_table)[index][0];

        if (index < MAX_SYMBOLS)
        {
            if (high_bits)
            {
                output_data[byte_pos] = (h4b_uint8_t)(index << 4);
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

