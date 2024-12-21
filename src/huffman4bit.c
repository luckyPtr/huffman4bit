#include "huffman4bit.h"
#include "stdio.h"
#include <stdint.h>
#include <string.h>


static huffman_tree_t huffman_tree;



static void cal_huffman_table(void)
{
    int i;
    int index;
    int parent;
    for (i = 0; i < MAX_SYMBOLS; i++)
    {
        index = i;
        while (huffman_tree.nodes[index].parent != -1)
        {
            parent = huffman_tree.nodes[index].parent;
            if (huffman_tree.nodes[parent].right == index)
            {
                huffman_tree.code_table[i].code |= (1 << huffman_tree.code_table[i].length);
            }
            huffman_tree.code_table[i].length++;
            index = parent;
        }
    }
}

// 构建编码表
static void buildHuffmanTable(int root, uint16_t code, uint8_t length) {
    if (root < MAX_SYMBOLS) {
        huffman_tree.code_table[root].code = code;
        huffman_tree.code_table[root].length = length;
        return;
    }

    buildHuffmanTable(huffman_tree.nodes[root].left, (code << 1) | 0, length + 1);
    buildHuffmanTable(huffman_tree.nodes[root].right, (code << 1) | 1, length + 1);
}

// 导出Huffman tree表 
// 仅保留非叶子节点
static void export_huffman_tree_table(h4b_uint8_t (*table)[2])
{
    h4b_uint8_t i;
    for (i = MAX_SYMBOLS; i < MAX_NODES; i++)
    {
        table[i - MAX_SYMBOLS][0] = (h4b_uint8_t)huffman_tree.nodes[i].left;
        table[i - MAX_SYMBOLS][1] = (h4b_uint8_t)huffman_tree.nodes[i].right;
    }
}

// 构建Huffman tree
void build_huffman_tree(void)
{
    h4b_int8_t node_count = MAX_SYMBOLS;
    h4b_int8_t min1, min2;
    while (node_count < MAX_NODES)
    {
        min1 = min2 = -1;
        // 找到频率最小的两个节点
        for (int i = 0; i < node_count; i++) 
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

    
}

void create_huffman_tree(h4b_uint8_t *huffman_tree_tbl)
{
    build_huffman_tree();
    cal_huffman_table();
    export_huffman_tree_table(huffman_tree_tbl);
}


static inline void set_bit(h4b_uint8_t *output_data, h4b_uint32_t pos, h4b_uint8_t bit)
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

void h4b_encode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_int8_t j;
    h4b_uint32_t bit_pos = 8;
    h4b_uint16_t code;
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
            set_bit(output_data, bit_pos, code & (1 << j));
        }

        code = huffman_tree.code_table[data_l].code;
        length = huffman_tree.code_table[data_l].length;
        for (j = length - 1; j >= 0; j--, bit_pos++)
        {
            set_bit(output_data, bit_pos, code & (1 << j));
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

void decode(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t (*huffman_tree)[2], h4b_uint8_t *output_data, h4b_uint32_t *output_len)
{
    h4b_uint32_t i;
    h4b_uint8_t byte;
    h4b_uint8_t index = MAX_NODES - MAX_SYMBOLS - 1;
    h4b_uint32_t byte_pos = 0;
    h4b_uint8_t high_bits = 1;
    h4b_uint8_t padding = input_data[0] & 0x07;
    h4b_uint8_t *data_ptr = &input_data[1];   // 第一个字节是头跳过
    h4b_uint32_t total_bits = ((input_len - 1) << 3) - padding;
    static H4B_CONST h4b_uint8_t shift_mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

    for (i = 0; i < total_bits; i++)
    {
        byte = data_ptr[i >> 3];
        //if ((byte << (i & 0x07)) & 0x80)
        if (byte & shift_mask[i & 0x07])
        {
            index = huffman_tree[index][1];
        }
        else
        {
            index = huffman_tree[index][0];
        }

        if (index < MAX_SYMBOLS)
        {
            output_data[byte_pos] = (high_bits) ? (index << 4) : (output_data[byte_pos] | index);
            byte_pos += high_bits ? 0 : 1;
            index = MAX_NODES - MAX_SYMBOLS - 1;
            high_bits ^= 1;  // 切换高低位标志
            // if (high_bits)
            // {
            //     output_data[byte_pos] = index << 4;
            // }
            // else
            // {
            //     output_data[byte_pos++] |= index;
            // }
            // index = MAX_NODES - MAX_SYMBOLS - 1;
            // high_bits = !high_bits;
        }
        else
        {
            index -= MAX_SYMBOLS;
        }
    }

    *output_len = byte_pos;
}


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
}

void h4b_cal_freq_append(h4b_uint8_t *data, h4b_uint32_t size)
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



void print_code(int code, int len)
{
    while (len)
    {
        printf("%d", (code & (1 << (len - 1))) ? 1 : 0);
        len--;
    }
    
}

void h4b_compress(h4b_uint8_t *input_data, h4b_uint32_t input_len, h4b_uint8_t *output_data, h4b_uint32_t *output_len, h4b_uint8_t *output_huffman_tree)
{
    h4b_reset_tree();
    h4b_cal_freq_append(input_data, input_len);
    create_huffman_tree(output_huffman_tree);
    
    for (int i = 0; i < 31; i++)
    {
        printf("%02d freq:%d parent:%d left:%d right:%d\n", i, huffman_tree.nodes[i].freq, huffman_tree.nodes[i].parent, huffman_tree.nodes[i].left, huffman_tree.nodes[i].right);
    }

    for (int i = 0; i < 16; i++)
    {
        printf("%02d %d %d ", i, huffman_tree.code_table[i].code, huffman_tree.code_table[i].length);
        print_code(huffman_tree.code_table[i].code, huffman_tree.code_table[i].length);
        printf("\n");
    }

    int new_size = 0;
    for (int i = 0; i < input_len; i++)
    {
        uint8_t h = input_data[i] >> 4;
        uint8_t l = input_data[i] & 0x0F;
        new_size += huffman_tree.code_table[h].length + huffman_tree.code_table[l].length;
    }
    printf("%d -> %d\n", input_len, (new_size + 7) / 8);
    h4b_encode(input_data, input_len, output_data, output_len);
    printf("End1\n");
}   

