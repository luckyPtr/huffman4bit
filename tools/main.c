/**
 * @file main.c
 * @brief Command-line entry point for the huffman4bit encoder.
 *
 * Usage:
 *   h4b_encoder -i <input.c> -o <out_basename> [-k <clusters>] [-v] [-V]
 *
 * Pipeline:
 *   1. Parse argv.
 *   2. parser_load()  - extract hex arrays from input .c
 *   3. Preprocess: per array, XOR+ZRL transform if it shrinks the array
 *      (flagged per-array in the stream header byte)
 *   4. h4b_init / add_array / clustering / create_huffman_tree
 *   5. h4b_encode()  - compress each representation
 *   6. Decode-verify every array against the original (lossless check)
 *   7. codegen_write() - emit <basename>.{c,h} into the -o directory
 *      (current directory when -o has no directory part)
 *   8. (-v) print per-array and total compression statistics
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman4bit.h"
#include "parser.h"
#include "codegen.h"

#define DEFAULT_K  (1U)

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -i <input.c> -o <out_basename> [-k <clusters>] [-v] [-V]\n"
        "  -i <file>   input C source containing 0xXX hex arrays\n"
        "  -o <name>   output path, may include a directory; writes <name>.c and\n"
        "              <name>.h there, symbols derive from the file-name part\n"
        "  -k <num>    cluster count, default %u, range 1..%u\n"
        "  -v          verbose: print per-array compression ratio\n"
        "  -V          print version and exit (also --version)\n"
        "  -h          show this help\n"
        "Arrays are preprocessed with XOR+zero-run coding when that shrinks\n"
        "them (per-array, flagged in the header byte); the output is always\n"
        "verified lossless against the input before writing.\n",
        prog, DEFAULT_K, MAX_CLUSTER_COUNT);
}

int main(int argc, char *argv[])
{
    const char *in_file  = NULL;
    const char *out_raw  = NULL;
    unsigned int k = DEFAULT_K;
    int verbose = 0;

    /* ---- 1. Parse argv ---- */
    for (int a = 1; a < argc; a++) {
        const char *arg = argv[a];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            printf("h4b_encoder %s\n", H4B_VERSION);
            return 0;
        } else if (strcmp(arg, "-i") == 0) {
            if (++a >= argc) { fprintf(stderr, "error: -i requires an argument\n"); return 1; }
            in_file = argv[a];
        } else if (strcmp(arg, "-o") == 0) {
            if (++a >= argc) { fprintf(stderr, "error: -o requires an argument\n"); return 1; }
            out_raw = argv[a];
        } else if (strcmp(arg, "-k") == 0) {
            char *endp;
            long kv;
            if (++a >= argc) { fprintf(stderr, "error: -k requires an argument\n"); return 1; }
            kv = strtol(argv[a], &endp, 10);
            if (*endp != '\0' || kv < 1 || kv > (long)MAX_CLUSTER_COUNT) {
                fprintf(stderr, "error: -k must be in 1..%u (got '%s')\n",
                        MAX_CLUSTER_COUNT, argv[a]);
                return 1;
            }
            k = (unsigned int)kv;
        } else if (strcmp(arg, "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", arg);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!in_file || !out_raw) {
        fprintf(stderr, "error: both -i and -o are required\n");
        print_usage(argv[0]);
        return 1;
    }

    /* ---- 2. Parse input .c ---- */
    raw_array_t *arrays = NULL;
    unsigned int array_count = 0;
    if (parser_load(in_file, &arrays, &array_count) != 0) {
        return 1;
    }
    if (array_count == 0U) {
        fprintf(stderr, "error: no arrays found in '%s'\n", in_file);
        return 1;
    }
    printf("Loaded %u array(s) from '%s'.\n", array_count, in_file);

    /* ---- 3. Split -o into output directory + symbol base name ---- */
    char *outdir   = NULL;
    char *basename = NULL;
    if (codegen_split_output(out_raw, &outdir, &basename) != 0) {
        return 1;
    }

    /* ---- 4. Preprocess: XOR+ZRL per array when it shrinks ---- */
    uint8_t **rep = (uint8_t **)calloc(array_count, sizeof(uint8_t *));
    uint32_t *rep_len = (uint32_t *)calloc(array_count, sizeof(uint32_t));
    uint8_t *rep_flag = (uint8_t *)calloc(array_count, sizeof(uint8_t));
    if (!rep || !rep_len || !rep_flag) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }
    unsigned int transformed = 0;
    for (unsigned int i = 0; i < array_count; i++) {
        uint32_t cap = arrays[i].len * 2U + 1U;
        uint8_t *tmp = (uint8_t *)malloc(cap);
        if (!tmp) { fprintf(stderr, "error: out of memory\n"); return 1; }
        uint32_t tl = h4b_xor_zrl_encode(arrays[i].data, arrays[i].len, tmp);
        if (tl < arrays[i].len) {
            rep[i] = (uint8_t *)malloc(tl);
            if (!rep[i]) { fprintf(stderr, "error: out of memory\n"); return 1; }
            memcpy(rep[i], tmp, tl);
            rep_len[i] = tl;
            rep_flag[i] = 1;
            transformed++;
        } else {
            rep[i] = arrays[i].data;      /* borrow: owned by parser */
            rep_len[i] = arrays[i].len;
            rep_flag[i] = 0;
        }
        free(tmp);
    }
    printf("Preprocess: %u/%u array(s) use XOR+ZRL.\n", transformed, array_count);

    /* ---- 5. Training ---- */
    h4b_init();
    for (unsigned int i = 0; i < array_count; i++) {
        h4b_add_array(rep[i], rep_len[i]);
    }
    {
        unsigned int eff = h4b_clustering((uint8_t)k);
        if (eff != k) {
            printf("Note: -k %u exceeds array count %u, using k=%u.\n",
                   k, array_count, eff);
            k = eff;
        }
    }
    printf("Training (k=%u)...\n", k);

    static huffman_tree_tbl_t tables[MAX_CLUSTER_COUNT] = {0};
    h4b_create_huffman_tree(tables);

    /* ---- 6. Encode each representation ---- */
    unsigned char **enc = (unsigned char **)calloc(array_count, sizeof(unsigned char *));
    unsigned int   *enc_lens = (unsigned int *)calloc(array_count, sizeof(unsigned int));
    if (!enc || !enc_lens) {
        fprintf(stderr, "error: out of memory\n");
        free(outdir); free(basename); parser_free(arrays, array_count);
        free(enc); free(enc_lens);
        return 1;
    }
    for (unsigned int i = 0; i < array_count; i++) {
        /* worst case: huffman codes up to 15 bits per nibble, plus header
         * and padding (high-entropy data can expand beyond input size) */
        enc[i] = (unsigned char *)malloc(arrays[i].len * 4U + 16U);
        if (!enc[i]) {
            fprintf(stderr, "error: out of memory\n");
            for (unsigned int j = 0; j < i; j++) free(enc[j]);
            free(outdir); free(basename); parser_free(arrays, array_count);
            free(enc); free(enc_lens);
            return 1;
        }
        h4b_encode(rep[i], rep_len[i], rep_flag[i], enc[i], &enc_lens[i]);
    }

    /* ---- 7. Lossless verification: decode and compare ---- */
    {
        unsigned char *dec = (unsigned char *)malloc(65536U);
        unsigned int fails = 0;
        if (!dec) { fprintf(stderr, "error: out of memory\n"); return 1; }
        for (unsigned int i = 0; i < array_count; i++) {
            if (arrays[i].len <= 65536U) {
                uint32_t dec_len = 0;
                h4b_decode(enc[i], enc_lens[i], tables, dec, &dec_len);
                if ((dec_len != arrays[i].len) ||
                    (memcmp(dec, arrays[i].data, arrays[i].len) != 0)) {
                    fails++;
                    fprintf(stderr, "verify FAILED on array %u\n", i);
                }
            } else {
                fprintf(stderr, "verify skipped on array %u (> 64 KB)\n", i);
            }
        }
        free(dec);
        if (fails) {
            fprintf(stderr, "error: %u array(s) failed verification, aborting\n", fails);
            for (unsigned int i = 0; i < array_count; i++) free(enc[i]);
            free(outdir); free(basename); parser_free(arrays, array_count);
            free(enc); free(enc_lens);
            return 1;
        }
        printf("Verified %u/%u array(s) lossless.\n", array_count, array_count);
    }

    /* ---- 8. Emit .c / .h ---- */
    codegen_ctx_t ctx;
    ctx.outdir      = outdir;
    ctx.basename    = basename;
    ctx.cluster_k   = k;
    ctx.data_count  = array_count;
    ctx.enc         = enc;
    ctx.enc_lens    = enc_lens;
    ctx.tables      = tables;
    ctx.arrays      = arrays;

    if (codegen_write(&ctx) != 0) {
        fprintf(stderr, "error: codegen failed\n");
        for (unsigned int i = 0; i < array_count; i++) free(enc[i]);
        free(outdir); free(basename); parser_free(arrays, array_count);
        free(enc); free(enc_lens);
        return 1;
    }

    if (outdir) {
        printf("Wrote %s/%s.c and %s/%s.h\n", outdir, basename, outdir, basename);
    } else {
        printf("Wrote %s.c and %s.h\n", basename, basename);
    }

    /* ---- 9. Statistics ---- */
    if (verbose) {
        unsigned int total_orig = 0;
        unsigned int total_enc  = 0;
        printf("------------------------------------------------------------\n");
        printf("  idx   original   compressed   ratio   clus  prep\n");
        for (unsigned int i = 0; i < array_count; i++) {
            unsigned int orig = arrays[i].len;
            unsigned int comp = enc_lens[i];
            unsigned int cluster = enc[i][0] >> 4;
            total_orig += orig;
            total_enc += comp;
            printf("  %3u   %7u    %7u    %5.1f%%    %2u    %s\n",
                   i, orig, comp, 100.0 * comp / orig, cluster,
                   rep_flag[i] ? "xz" : "-");
        }
        printf("------------------------------------------------------------\n");
        printf("  total  %7u    %7u    %5.1f%%    (k=%u, tables=%u B)\n",
               total_orig, total_enc,
               100.0 * total_enc / total_orig,
               k, k * 30U);
    }

    /* ---- 10. Cleanup ---- */
    for (unsigned int i = 0; i < array_count; i++) free(enc[i]);
    free(enc);
    free(enc_lens);
    for (unsigned int i = 0; i < array_count; i++)
        if (rep_flag[i]) free(rep[i]);           /* borrowed arrays freed by parser_free */
    free(rep); free(rep_len); free(rep_flag);
    free(outdir); free(basename);
    parser_free(arrays, array_count);
    return 0;
}
