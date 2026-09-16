#ifndef __ENCODER_CODEGEN_H__
#define __ENCODER_CODEGEN_H__

#include "huffman4bit.h"
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Context describing everything codegen needs to emit the .c/.h pair.
 */
typedef struct {
    const char               *outdir;       /* output directory, NULL = CWD    */
    const char               *basename;     /* sanitised file-name part of -o  */
    unsigned int              cluster_k;    /* number of clusters used (1..16) */
    unsigned int              data_count;   /* number of compressed arrays     */
    unsigned char           **enc;          /* [data_count] compressed buffers */
    unsigned int             *enc_lens;     /* [data_count] their byte lengths */
    const huffman_tree_tbl_t *tables;       /* [cluster_k] Huffman tables      */
    const raw_array_t        *arrays;       /* [data_count] source arrays (names/groups) */
} codegen_ctx_t;

/**
 * @brief Sanitise an arbitrary string into a valid C identifier fragment.
 *
 * - Non-[A-Za-z0-9_] characters become '_'.
 * - Leading digit is prefixed with '_'.
 * Caller must free() the returned string.
 */
char *codegen_sanitize_basename(const char *raw);

/**
 * @brief Split a -o argument into its directory part and symbol base name.
 *
 * "dir/name" -> outdir "dir", basename sanitised from "name"
 * "name"     -> outdir NULL,  basename sanitised from "name"
 * "dir/"     -> error (no file name after the directory part)
 * Both '/' and '\' are accepted as separators. The two outputs are
 * malloc'd (outdir is NULL when absent); caller must free() them.
 * @return 0 on success, -1 on malformed input or OOM (message on stderr).
 */
int codegen_split_output(const char *raw, char **outdir, char **basename);

/**
 * @brief Write <basename>.h and <basename>.c into outdir (or the current
 *        working directory when outdir is NULL).
 * @return 0 on success, -1 on error (message printed to stderr).
 */
int codegen_write(const codegen_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_CODEGEN_H__ */
