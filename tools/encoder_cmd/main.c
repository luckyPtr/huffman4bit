#include "stdio.h"
#include "huffman4bit.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "pcre2.h"
#include "string.h"

/************************
 * Function Definitions *
 ************************/

#define BUFFER_SIZE 10000
typedef unsigned char u8;

void extract_hex_arrays(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, BUFFER_SIZE - 1, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    pcre2_code *re;
    PCRE2_SPTR pattern = (PCRE2_SPTR)"\\{\\s*(0x[0-9A-Fa-f]{1,2}\\s*,\\s*)*(0x[0-9A-Fa-f]{1,2})?\\s*\\}";   // 匹配十六进制数组 {0x00, 0x01, 0x02, 0x03}
    int errornumber;
    PCRE2_SIZE erroroffset;
    re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);

    if (!re) {
        fprintf(stderr, "PCRE2 compilation failed\n");
        return;
    }

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);

    int rc;
    PCRE2_SIZE *ovector;
    PCRE2_SPTR subject = (PCRE2_SPTR)buffer;
    PCRE2_SIZE subject_length = bytes_read;

    rc = pcre2_match(re, subject, subject_length, 0, 0, match_data, NULL);

    while (rc >= 0) {
        ovector = pcre2_get_ovector_pointer(match_data);
        printf("Match found:\n");

        for (PCRE2_SIZE i = ovector[0] + 1; i < ovector[1] - 1; i++) {
            putchar(buffer[i]);
        }
        printf("\n");

        rc = pcre2_match(re, subject, subject_length, ovector[1], 0, match_data, NULL);
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
}


int main(int argc, char *argv[]) {
    // if (argc != 2) {
    //     printf("Usage: %s <filename>\n", argv[0]);
    //     return 1;
    // }
    char *filename = "C:\\Users\\Naive\\Desktop\\1\\bm_img.c";
    count_hex_frequencies(filename);
    printf("Done.\n");
    return 0;
}


void count_hex_frequencies(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char *buffer = NULL;
    size_t bytes_read;

    // 使用 fseek 移动文件指针到文件末尾，计算文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);  // 获取文件指针当前位置，即文件大小
    rewind(file);  // 将文件指针移动回文件开头

    buffer = malloc(file_size + 1);
    if (!buffer) {
        perror("Failed to allocate memory");
        fclose(file);
        return;
    }

    bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    printf("Read %zu bytes\n", bytes_read);
    for (size_t i = 0; i < bytes_read; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");

}   