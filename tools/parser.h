#ifndef __ENCODER_PARSER_H__
#define __ENCODER_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A single extracted bitmap array (raw uncompressed bytes).
 */
typedef struct {
    unsigned char *data;   /* heap-allocated byte buffer */
    unsigned int  len;     /* number of valid bytes in data */
    char          *name;   /* declarator name from the source ("FOO" in "FOO[] = {...}"),
                            * heap string shared by every member of a 2D group; may be NULL */
    unsigned int  group;   /* outer-initializer id: arrays from the same "= {...}" share it */
    unsigned int  sub;     /* index within its group (0 for 1D arrays) */
    unsigned char nested;  /* 1 = came from an inner {...} of a 2D array (Shape A) */
} raw_array_t;

/**
 * @brief Parse a C source file and extract all top-level hex arrays.
 *
 * Supports two input shapes (auto-detected):
 *   - Shape A (2D bitmap array, e.g. bm_img.c):
 *       const unsigned char ARR[][N] = { {0x.., ...}, {0x.., ...}, ... };
 *     Each inner {...} becomes one raw_array_t.
 *   - Shape B (1D array):
 *       const unsigned char ARR[] = { 0x.., 0x.., ... };
 *     The single {...} becomes one raw_array_t.
 *
 * Function bodies ({...} not preceded by '=') are skipped automatically.
 *
 * @param filename  Path to the input .c file.
 * @param out_arrays  On success, receives a heap-allocated array of raw_array_t.
 * @param out_count   On success, receives the number of extracted arrays (>=1).
 * @return 0 on success, -1 on error (message printed to stderr).
 */
int parser_load(const char *filename,
                raw_array_t **out_arrays,
                unsigned int *out_count);

/**
 * @brief Release memory allocated by parser_load.
 */
void parser_free(raw_array_t *arrays, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_PARSER_H__ */
