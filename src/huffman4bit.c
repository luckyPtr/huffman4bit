/**
 * Huffman4bit algorithm implementation.
 */

#include "huffman4bit.h"

#if USE_ENCODE
#include <string.h>
#include <math.h>
#include <stdlib.h>
/**********************
 *  STATIC VARIABLES
 **********************/
static huffman_tree_t huffman_tree[MAX_CLUSTER_COUNT];     /* Huffman tree for each cluster */
static centroid_t centroids;
static input_buffer_t input_buffer;

/********************** 
 *  STATIC PROTOTYPES
 **********************/
static void cal_huffman_table(uint8_t k);
static void export_huffman_tree_table(uint8_t k, huffman_tree_tbl_t *table);
static void build_huffman_tree(uint8_t k);
static double euclidean_distance(symbol_frequencies_t p1, symbol_frequencies_t p2);
static void count_freq_append(uint8_t k, uint8_t *data, uint32_t size);
static void assign_points(void);
static void update_centroids(void);
static void calculate_features(void);
static void init_centroids(uint8_t k);
static void k_means(void);
static uint8_t find_nearest_cluster(uint8_t *input_data, uint32_t input_len);
static void set_bit(uint8_t *output_data, uint32_t pos, uint8_t bit);

/**********************
 *   STATIC FUNCTIONS
 **********************/
/**
 * @brief Calculate the Huffman codes for each symbol in the input data.
 * 
 * @param k Cluster index.
 */
static void cal_huffman_table(uint8_t k)
{
    uint8_t i;
    uint8_t index;
    uint8_t parent;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        index = i;
        while (huffman_tree[k].nodes[index].parent != -1)
        {
            parent = (uint8_t)huffman_tree[k].nodes[index].parent;
            if (huffman_tree[k].nodes[parent].right == index)
            {
                huffman_tree[k].code_table[i].code |= ((uint32_t)1 << huffman_tree[k].code_table[i].length);
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
static void export_huffman_tree_table(uint8_t k, huffman_tree_tbl_t *table)
{
    uint8_t i;
    for (i = MAX_SYMBOLS; i < MAX_NODES; i++)
    {
        (*table)[i - MAX_SYMBOLS][0] = (uint8_t)huffman_tree[k].nodes[i].left;
        (*table)[i - MAX_SYMBOLS][1] = (uint8_t)huffman_tree[k].nodes[i].right;
    }
}

/**
 * @brief Build the Huffman tree for the input data.
 * 
 * @param k Cluster index.
 */
static void build_huffman_tree(uint8_t k)
{
    int8_t node_count = MAX_SYMBOLS;
    int8_t min1, min2;
    int8_t i;
    
    while ((uint8_t)node_count < MAX_NODES)
    {
        min1 = min2 = -1;
        /* find the two nodes with the smallest frequency */
        for (i = 0; i < node_count; i++) 
        {
            if (huffman_tree[k].nodes[(uint8_t)i].parent == -1) 
            {
                if (min1 == -1 || huffman_tree[k].nodes[(uint8_t)i].freq < huffman_tree[k].nodes[(uint8_t)min1].freq) 
                {
                    min2 = min1;
                    min1 = i;
                } 
                else if (min2 == -1 || huffman_tree[k].nodes[(uint8_t)i].freq < huffman_tree[k].nodes[(uint8_t)min2].freq) 
                {
                    min2 = i;
                }
            }
        }

        /* merge the two nodes */
        huffman_tree[k].nodes[(uint8_t)node_count].freq = huffman_tree[k].nodes[(uint8_t)min1].freq + huffman_tree[k].nodes[(uint8_t)min2].freq;
        huffman_tree[k].nodes[(uint8_t)node_count].left = min1;
        huffman_tree[k].nodes[(uint8_t)node_count].right = min2;
        huffman_tree[k].nodes[(uint8_t)node_count].parent = -1;

        huffman_tree[k].nodes[(uint8_t)min1].parent = node_count;
        huffman_tree[k].nodes[(uint8_t)min2].parent = node_count;

        node_count++;
    }
}


/**
 * @brief Calculate the Euclidean distance between two data points.
 * 
 * @param p1 Pointer to the first data point.
 * @param p2 Pointer to the second data point.
 * @return double Euclidean distance between the two data points.
 */
static double euclidean_distance(symbol_frequencies_t p1, symbol_frequencies_t p2)
{
    uint8_t i;
    double sum = 0;
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
static void count_freq_append(uint8_t k, uint8_t *data, uint32_t size)
{
    uint8_t h, l;
    uint32_t i;
    
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
    uint32_t i;
    uint8_t j;
    double min_dist, dist;
    uint8_t min_cluster;

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
    uint32_t i, j;
    uint32_t cluster_size[MAX_CLUSTER_COUNT] = {0};
    symbol_frequencies_t sum[MAX_CLUSTER_COUNT] = {0};

    /* 计算每个簇的大小和特征值之和 */
    for (i = 0; i < input_buffer.array_count; i++)
    {
        cluster_size[input_buffer.array_data[i].cluster_id]++;
        for (j = 0; j < MAX_SYMBOLS; j++)
        {
            sum[input_buffer.array_data[i].cluster_id][j] += input_buffer.array_data[i].feature[j];
        }
    }

    /* 更新质心 */
    for (i = 0; i < centroids.points_count; i++)
    {
        if (cluster_size[i] == 0U)
        {
            continue;   /* 空簇：保留上一轮质心，避免 0 除产生 NaN */
        }
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
    uint32_t i, j;
    uint32_t data_count[MAX_SYMBOLS];
    uint32_t total_count;
    uint8_t h, l;
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

        if (total_count == 0U)
        {
            memset(array->feature, 0, sizeof(array->feature));   /* 空数组：特征全零 */
            continue;
        }

        for (j = 0; j < MAX_SYMBOLS; j++)
        {
            array->feature[j] = (double)data_count[j] / total_count;
        }

    }
}

/**
 * @brief Initialize the centroids.
 *
 * Seeds are deterministically evenly spaced over the input order
 * (cluster i starts at array i * N / k). Unlike the previous
 * srand(time(NULL)) pick this makes the whole encoder reproducible:
 * the same input and k always yield the same clusters, Huffman
 * tables and compressed output.
 *
 * @param k Number of centroids.
 */
static void init_centroids(uint8_t k)
{
    uint8_t i;
    uint32_t step;

    if (k > MAX_CLUSTER_COUNT)
    {
        k = MAX_CLUSTER_COUNT;
    }
    if ((uint32_t)k > input_buffer.array_count)
    {
        k = (uint8_t)input_buffer.array_count;   /* 多于数组数时无法选出 k 个不同索引 */
    }
    if (k == 0U)
    {
        centroids.points_count = 0;
        return;
    }

    step = input_buffer.array_count / k;

    for (i = 0; i < k; i++)
    {
        centroids.point_index[i] = (uint32_t)i * step;
        memcpy(centroids.centroid_points[i],
               input_buffer.array_data[centroids.point_index[i]].feature,
               sizeof(symbol_frequencies_t));
    }
    centroids.points_count = k;
}

/**
 * @brief Perform K-means clustering on the input data.
 */
static void k_means(void)
{
    uint32_t iter = 0;
    uint8_t changed;
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
 * @return uint8_t Nearest cluster index.
 */
static uint8_t find_nearest_cluster(uint8_t *input_data, uint32_t input_len)
{
    uint32_t i;
    uint8_t h, l;
    uint32_t data_count[MAX_SYMBOLS];
    uint32_t total_count;
    symbol_frequencies_t feature;
    double min_dist, dist;
    uint8_t min_cluster;

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

    if (total_count == 0U)
    {
        return 0;   /* 空数组：无特征可用，归入首簇 */
    }

    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        feature[i] = (double)data_count[i] / total_count;
    }

    min_dist = euclidean_distance(feature, centroids.centroid_points[0]);
    min_cluster = 0;

    for (i = 1; i < centroids.points_count; i++)
    {
        dist = euclidean_distance(feature, centroids.centroid_points[i]);
        if (dist < min_dist)
        {
            min_dist = dist;
            min_cluster = (uint8_t)i;
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
    uint8_t i, k;
    uint32_t j;
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

    /* 释放原始数组 */
    if (input_buffer.array_data != NULL)
    {
        for (j = 0; j < input_buffer.array_count; j++)
        {
            free(input_buffer.array_data[j].array_ptr);
        }
        free(input_buffer.array_data);
        input_buffer.array_data = NULL;
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
void h4b_add_array(uint8_t *input_data, uint32_t input_len)
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
    new_array->array_ptr = (uint8_t*)malloc(input_len);
    memcpy(new_array->array_ptr, input_data, input_len);
    input_buffer.array_count++;
}



/**
 * @brief Perform clustering on the input data.
 *
 * @param k number of centroids.
 * @return Effective cluster count actually used: k clamped to the number
 *         of added arrays (and to MAX_CLUSTER_COUNT); 0 if no array was added.
 */
uint8_t h4b_clustering(uint8_t k)
{
    if (input_buffer.array_count == 0U)
    {
        centroids.points_count = 0;
        return 0;
    }

    calculate_features();   /* 计算特征 */
    init_centroids(k);      /* 确定性初始化质心 */
    k_means();              /* K-means算法 */

    return centroids.points_count;
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
    uint8_t i;
    uint32_t j;

    for (i = 0; i < centroids.points_count; i++)    /* create huffman tree for each cluster */
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
static void set_bit(uint8_t *output_data, uint32_t pos, uint8_t bit)
{
    if (bit)
    {
        output_data[pos >> 3] |= (uint8_t)(1 << (7 - (pos & 0x07)));
    }
    else
    {
        output_data[pos >> 3] &= (uint8_t)~(1 << (7 - (pos & 0x07)));
    }
}

/**
 * @brief Per-array preprocessing: XOR with previous byte, then zero-run
 *        coding. A run of >= 2 zero bytes becomes the 3-byte marker
 *        00 00 (cnt-2); a lone zero stays literal. The coding is
 *        self-delimiting: the encoder never emits two adjacent literal
 *        zeros, so "00 00" in the stream is always a marker.
 *
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data.
 * @param output_data Pointer to the output buffer (needs 2*input_len bytes).
 * @return Length of the transformed data.
 */
uint32_t h4b_xor_zrl_encode(const uint8_t *input_data, uint32_t input_len,
                                uint8_t *output_data)
{
    uint32_t i = 0;
    uint32_t o = 0;

    while (i < input_len)
    {
        /* transformed byte y[i] = x[i] ^ x[i-1]: y == 0 <=> x[i] == x[i-1]
         * (y[0] = x[0], so a leading 0x00 byte is also a transformed zero
         * and must join the run - otherwise it would be emitted as a lone
         * literal zero directly before a "00 00 cnt" marker, breaking the
         * self-delimiting invariant) */
        if ((i == 0 && input_data[0] == 0x00) ||
            (i > 0 && input_data[i] == input_data[i - 1]))
        {
            uint32_t run = 1;
            uint32_t r;

            while ((i + run < input_len) && (input_data[i + run] == input_data[i + run - 1]))
            {
                run++;
            }
            if (run >= 2)
            {
                r = run;
                for (;;)
                {
                    if (r >= 2)
                    {
                        uint32_t cnt = (r - 2 > 255) ? 255 : (r - 2);
                        output_data[o++] = 0x00;
                        output_data[o++] = 0x00;
                        output_data[o++] = (uint8_t)cnt;
                        r -= cnt + 2;
                        if (r == 0) break;
                        if (r == 1) { output_data[o++] = 0x00; break; }
                    }
                    else
                    {
                        output_data[o++] = 0x00;
                        break;
                    }
                }
                i += run;
                continue;
            }
            /* lone transformed zero */
            output_data[o++] = 0x00;
            i++;
            continue;
        }
        output_data[o++] = (uint8_t)(input_data[i] ^ (i ? input_data[i - 1] : 0));
        i++;
    }
    return o;
}

/**
 * @brief Encode the input data using the nearest Huffman tree.
 *
 * @param input_data Pointer to the input data array (already preprocessed
 *                   when transform is set).
 * @param input_len Length of the input data.
 * @param transform 1 if input_data is in XOR+ZRL domain, 0 for raw data.
 * @param output_data Pointer to the output data array.
 * @param output_len Pointer to the length of the output data.
 */
void h4b_encode(uint8_t *input_data, uint32_t input_len, uint8_t transform,
                uint8_t *output_data, uint32_t *output_len)
{
    uint32_t i;
    int32_t j;
    uint32_t bit_pos = 8;
    uint32_t code;
    uint8_t length;
    uint8_t padding;
    uint8_t cluster;
    uint8_t data_h, data_l;
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
    /* header byte: [cluster:4][transform:1][padding:3] */
    output_data[0] = (uint8_t)((cluster << 4) | ((transform & 1U) << 3) | padding);

    *output_len =  bit_pos / 8;
}


#endif /* !USE_ENCODE */


/* ------------------------------------------------------------------ */
/*  Decode side                                                        */
/*                                                                    */
/*  Stream header byte: [cluster:4][transform:1][padding:3]           */
/*                                                                    */
/*  Decoding is a purely sequential byte chain with no read-back:      */
/*    Huffman bit-walk -> (transform? ZRL expand + XOR undo) -> emit   */
/*  so the streaming variant can write straight to a display bus       */
/*  with no intermediate buffer.  The input blob is trusted: it is     */
/*  produced by h4b_encoder and verified lossless at build time, so    */
/*  the decoders carry no size-costing bounds checks.                  */
/* ------------------------------------------------------------------ */

typedef struct
{
    h4b_emit_fn emit;
    void *ctx;
    uint8_t transform;   /* header flag                            */
    uint8_t state;       /* ZRL: 0=normal 1=saw one 0 2=need count */
    uint8_t prev;        /* previous FINAL byte (XOR undo)          */
} h4b_sink_t;

/* undo one XOR step and forward the final byte */
static void sink_xor_emit(h4b_sink_t *s, uint8_t t)
{
    uint8_t b = (uint8_t)(t ^ s->prev);
    s->prev = b;
    s->emit(s->ctx, b);
}

/* feed one byte of the (possibly transformed) huffman output */
static void sink_byte(h4b_sink_t *s, uint8_t b)
{
    if (!s->transform)
    {
        s->emit(s->ctx, b);
        return;
    }
    switch (s->state)
    {
    case 0:
        if (b == 0x00) { s->state = 1; return; }
        sink_xor_emit(s, b);
        return;
    case 1:
        if (b == 0x00) { s->state = 2; return; }   /* "00 00": marker */
        sink_xor_emit(s, 0x00);                    /* lone literal zero */
        sink_xor_emit(s, b);
        s->state = 0;
        return;
    default:                                        /* b is the run count */
    {
        uint16_t n = (uint16_t)(b + 2U);
        while (n--) sink_xor_emit(s, 0x00);
        s->state = 0;
        return;
    }
    }
}

/* end of stream: a pending lone zero still needs to be emitted */
static void sink_flush(h4b_sink_t *s)
{
    if (s->transform && (s->state == 1))
    {
        sink_xor_emit(s, 0x00);
    }
}

/**
 * @brief Streaming decode: bytes are handed to the caller's emit callback
 *        as soon as they are reconstructed.
 *
 * @param input_data Compressed array (header byte + huffman bits).
 * @param input_len Length of the compressed array.
 * @param table Huffman tree tables (from codegen output).
 * @param emit Byte-sink callback.
 * @param ctx Opaque context passed through to the callback.
 */
void h4b_decode_stream(const uint8_t *input_data, uint32_t input_len,
                       const huffman_tree_tbl_t *table,
                       h4b_emit_fn emit, void *ctx)
{
    uint32_t i;
    uint8_t byte;
    uint8_t index = MAX_NODES - MAX_SYMBOLS - 1;
    uint8_t staged = 0;
    uint8_t high_bits = 1;
    uint8_t padding = (uint8_t)(input_data[0] & 0x07);
    const uint8_t *data_ptr = &input_data[1];
    uint32_t total_bits = ((input_len - 1) << 3) - padding;
    const huffman_tree_tbl_t *current_table = &table[input_data[0] >> 4];
    h4b_sink_t sink;

    sink.emit = emit;
    sink.ctx = ctx;
    sink.transform = (uint8_t)((input_data[0] >> 3) & 1U);
    sink.state = 0;
    sink.prev = 0;

    for (i = 0; i < total_bits; i++)
    {
        byte = data_ptr[i >> 3];
        index = ((byte << (i & 0x07)) & 0x80) ? (*current_table)[index][1] : (*current_table)[index][0];

        if (index < MAX_SYMBOLS)
        {
            if (high_bits)
            {
                staged = (uint8_t)(index << 4);
            }
            else
            {
                sink_byte(&sink, (uint8_t)(staged | index));
            }
            index = MAX_NODES - MAX_SYMBOLS - 1;
            high_bits ^= 1;
        }
        else
        {
            index -= MAX_SYMBOLS;
        }
    }

    sink_flush(&sink);
}

/**
 * @brief Decode the input data using the Huffman tree table.
 *
 * Direct buffer implementation: same walk/ZRL/XOR chain as
 * h4b_decode_stream, but writing straight into the output buffer with
 * no callback indirection - smaller code for buffer-based projects.
 *
 * @param input_data Pointer to the input data array.
 * @param input_len Length of the input data array.
 * @param table Pointer to the Huffman tree table.
 * @param output_data Pointer to the output data array (must be large
 *                    enough for the ORIGINAL, i.e. post-transform, data).
 * @param output_len Pointer to the length of the output data.
 */
void h4b_decode(const uint8_t *input_data, uint32_t input_len,
                const huffman_tree_tbl_t *table,
                uint8_t *output_data, uint32_t *output_len)
{
    uint32_t i;
    uint32_t o = 0;
    uint8_t byte;
    uint8_t index = MAX_NODES - MAX_SYMBOLS - 1;
    uint8_t staged = 0;
    uint8_t high_bits = 1;
    uint8_t transform = (uint8_t)((input_data[0] >> 3) & 1U);
    uint8_t padding = (uint8_t)(input_data[0] & 0x07);
    uint8_t state = 0;                /* ZRL: 0=normal 1=saw one 0 2=need count */
    uint8_t prev = 0;                 /* previous FINAL byte (XOR undo)          */
    const uint8_t *data_ptr = &input_data[1];
    uint32_t total_bits = ((input_len - 1) << 3) - padding;
    const huffman_tree_tbl_t *current_table = &table[input_data[0] >> 4];

    for (i = 0; i < total_bits; i++)
    {
        byte = data_ptr[i >> 3];
        index = ((byte << (i & 0x07)) & 0x80) ? (*current_table)[index][1] : (*current_table)[index][0];

        if (index < MAX_SYMBOLS)
        {
            if (high_bits)
            {
                staged = (uint8_t)(index << 4);
            }
            else
            {
                uint8_t t = (uint8_t)(staged | index);
                if (!transform)
                {
                    output_data[o++] = t;
                }
                else if (state == 0)
                {
                    if (t == 0x00) { state = 1; }
                    else { prev = (uint8_t)(t ^ prev); output_data[o++] = prev; }
                }
                else if (state == 1)
                {
                    if (t == 0x00) { state = 2; }         /* "00 00": marker */
                    else
                    {
                        output_data[o++] = prev;          /* lone literal zero */
                        prev = (uint8_t)(t ^ prev);
                        output_data[o++] = prev;
                        state = 0;
                    }
                }
                else
                {
                    uint16_t n = (uint16_t)(t + 2U);
                    while (n--) { output_data[o++] = prev; }
                    state = 0;
                }
            }
            index = MAX_NODES - MAX_SYMBOLS - 1;
            high_bits ^= 1;
        }
        else
        {
            index -= MAX_SYMBOLS;
        }
    }

    /* a pending lone transformed zero still needs to be emitted */
    if (transform && (state == 1))
    {
        output_data[o++] = prev;
    }

    *output_len = o;
}
