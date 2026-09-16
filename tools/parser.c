/**
 * @file parser.c
 * @brief Pure-C scanner for extracting top-level hex arrays from a .c source.
 *
 * Design:
 *   - Single-pass character scanner with a small state machine.
 *   - Skips line comments, block comments, string and char literals.
 *   - Tracks curly-brace nesting depth.
 *   - Recognises "array initialisation" by the pattern:  = { ... }
 *       If the outer { directly contains 0xXX bytes  -> Shape B (1 array).
 *       If the outer { contains nested { ... } blocks  -> Shape A (N arrays).
 *   - Function bodies ( { ... } without preceding '=') are ignored.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

/* ------------------------------------------------------------------ */
/*  Dynamic byte buffer used while collecting one array               */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *data;
    unsigned int  len;
    unsigned int  cap;
} byte_buf_t;

static int buf_reserve(byte_buf_t *b, unsigned int need)
{
    unsigned char *p;
    if (b->cap >= need) return 0;
    if (b->cap == 0) b->cap = 256;
    while (b->cap < need) b->cap *= 2;
    p = (unsigned char *)realloc(b->data, b->cap);
    if (!p) return -1;
    b->data = p;
    return 0;
}

static int buf_push(byte_buf_t *b, unsigned char v)
{
    if (buf_reserve(b, b->len + 1) != 0) return -1;
    b->data[b->len++] = v;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Dynamic array of raw_array_t                                       */
/* ------------------------------------------------------------------ */

static int arrays_push(raw_array_t **out_arr, unsigned int *out_n,
                       unsigned int *out_cap, raw_array_t item)
{
    raw_array_t *p;
    if (*out_n >= *out_cap) {
        *out_cap = (*out_cap == 0) ? 16 : (*out_cap * 2);
        p = (raw_array_t *)realloc(*out_arr, sizeof(raw_array_t) * (*out_cap));
        if (!p) return -1;
        *out_arr = p;
    }
    (*out_arr)[(*out_n)++] = item;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Character classification helpers                                  */
/* ------------------------------------------------------------------ */

static int is_hex_digit(int c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int hex_val(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}

static int is_ident_start(int c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c == '_');
}

static int is_ident_char(int c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

/* bounded strdup (C89 has no strdup) */
static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ------------------------------------------------------------------ */
/*  Parser load                                                       */
/* ------------------------------------------------------------------ */

int parser_load(const char *filename,
                raw_array_t **out_arrays,
                unsigned int *out_count)
{
    FILE *f;
    long  fsize;
    char *src;
    size_t nread;
    unsigned int i;

    raw_array_t *arrays = NULL;
    unsigned int arrays_n = 0;
    unsigned int arrays_cap = 0;

    /* scanner state */
    int    depth = 0;            /* current { } nesting depth (0 = top level) */
    int    after_equals = 0;     /* 1 if we've recently seen '=' (until '{') */
    int    in_initializer = 0;   /* 1 while we are inside the outer {...} of an = */
    int    depth_one_direct_hex = 0; /* 1 if a hex seen at depth==1 in current init */
    int    depth_one_saw_nested = 0; /* 1 if a nested { seen at depth==1 in current init */
    byte_buf_t cur;              /* buffer for the current inner block */
    int    have_cur = 0;         /* 1 if cur holds any bytes */

    /* declarator-name capture: accumulate identifier tokens, remember the last
     * valid one, and bind it to the initializer when its '{' opens */
    char   ident_buf[256];
    size_t ident_len = 0;
    char   last_ident[256];
    int    have_last_ident = 0;
    const char *group_name = NULL;  /* declarator of the current initializer */
    unsigned int group_seq = 0;     /* id of the current outer initializer */
    unsigned int nested_idx = 0;    /* inner-block counter within the initializer */

    cur.data = NULL; cur.len = 0; cur.cap = 0;

    *out_arrays = NULL;
    *out_count  = 0;

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "parser: cannot open '%s'\n", filename);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    rewind(f);
    if (fsize <= 0) {
        fprintf(stderr, "parser: empty file '%s'\n", filename);
        fclose(f);
        return -1;
    }
    src = (char *)malloc((size_t)fsize + 1);
    if (!src) {
        fprintf(stderr, "parser: out of memory\n");
        fclose(f);
        return -1;
    }
    nread = fread(src, 1, (size_t)fsize, f);
    src[nread] = '\0';
    fclose(f);

    /* Scan character by character */
    for (size_t p = 0; p < nread; ) {
        int c = (unsigned char)src[p];

        /* ---- comments & literals (skip regardless of depth) ---- */
        if (c == '/' && p + 1 < nread && src[p + 1] == '/') {
            p += 2;
            while (p < nread && src[p] != '\n') p++;
            continue;
        }
        if (c == '/' && p + 1 < nread && src[p + 1] == '*') {
            p += 2;
            while (p + 1 < nread && !(src[p] == '*' && src[p + 1] == '/')) p++;
            p += 2;
            if (p > nread) p = nread;
            continue;
        }
        if (c == '"') {
            p++;
            while (p < nread && src[p] != '"') {
                if (src[p] == '\\' && p + 1 < nread) p += 2;
                else p++;
            }
            if (p < nread) p++;
            continue;
        }
        if (c == '\'') {
            p++;
            while (p < nread && src[p] != '\'') {
                if (src[p] == '\\' && p + 1 < nread) p += 2;
                else p++;
            }
            if (p < nread) p++;
            continue;
        }

        /* ---- hex byte: 0x[0-9A-Fa-f]{1,2} (must precede ident tracking: its
         * digits are also identifier characters) ---- */
        if (c == '0' && p + 1 < nread && (src[p + 1] == 'x' || src[p + 1] == 'X')) {
            int v = 0;
            int digits = 0;
            size_t q = p + 2;
            while (q < nread && is_hex_digit((unsigned char)src[q]) && digits < 2) {
                v = (v << 4) | hex_val((unsigned char)src[q]);
                digits++;
                q++;
            }
            if (digits == 0) { p++; continue; }   /* "0x" not followed by hex - skip */
            if (v > 0xFF) {
                /* shouldn't happen since we cap at 2 digits */
                fprintf(stderr, "parser: hex value > 0xFF at offset %zu\n", p);
                goto fail;
            }
            if (in_initializer && (depth == 1 || depth == 2)) {
                if (depth == 1) depth_one_direct_hex = 1;
                if (buf_push(&cur, (unsigned char)v) != 0) {
                    fprintf(stderr, "parser: out of memory\n");
                    goto fail;
                }
                have_cur = 1;
            }
            p = q;
            continue;
        }

        /* ---- identifier token accumulation (declarator name capture) ---- */
        if (is_ident_char(c)) {
            if (ident_len < sizeof(ident_buf) - 1) ident_buf[ident_len++] = (char)c;
            p++;
            continue;
        }
        if (ident_len > 0) {
            /* flush the pending token; keep only valid identifiers so array
             * dimensions like the 1024 in FOO[][1024] never win */
            if (is_ident_start((unsigned char)ident_buf[0])) {
                ident_buf[ident_len] = '\0';
                memcpy(last_ident, ident_buf, ident_len + 1);
                have_last_ident = 1;
            }
            ident_len = 0;
        }

        /* ---- '=' tracking (only meaningful at top level) ---- */
        if (depth == 0 && c == '=') {
            after_equals = 1;
            p++;
            continue;
        }

        /* ---- '{' handling ---- */
        if (c == '{') {
            if (depth == 0) {
                if (!after_equals) {
                    /* function body / unrelated block -> ignore entirely */
                    depth++;
                    p++;
                    continue;
                }
                /* entering an outer initializer */
                in_initializer = 1;
                depth_one_direct_hex = 0;
                depth_one_saw_nested = 0;
                group_seq++;
                nested_idx = 0;
                group_name = have_last_ident ? last_ident : NULL;
                depth++;
                p++;
                continue;
            }
            /* depth >= 1 */
            if (depth == 1 && in_initializer) {
                /* beginning of a nested block = one array (Shape A) */
                depth_one_saw_nested = 1;
                cur.len = 0;
                have_cur = 0;
            }
            depth++;
            p++;
            continue;
        }

        /* ---- '}' handling ---- */
        if (c == '}') {
            if (depth == 1 && in_initializer) {
                /* closing the outer initializer */
                if (depth_one_saw_nested == 0 && depth_one_direct_hex != 0) {
                    /* Shape B: entire depth-1 was one array */
                    if (have_cur && cur.len > 0) {
                        raw_array_t item;
                        char *nm;
                        nm = (group_name != NULL) ? dup_str(group_name) : NULL;
                        if (group_name != NULL && nm == NULL) {
                            fprintf(stderr, "parser: out of memory\n");
                            goto fail;
                        }
                        item.data = cur.data;
                        item.len  = cur.len;
                        item.name = nm;
                        item.group = group_seq;
                        item.sub  = 0;
                        item.nested = 0;
                        cur.data = NULL; cur.len = 0; cur.cap = 0;
                        have_cur = 0;
                        if (arrays_push(&arrays, &arrays_n, &arrays_cap, item) != 0) {
                            fprintf(stderr, "parser: out of memory\n");
                            goto fail;
                        }
                    }
                }
                in_initializer = 0;
                after_equals   = 0;
                group_name     = NULL;
                have_last_ident = 0;
            }
            else if (depth == 2 && in_initializer && depth_one_saw_nested) {
                /* closing a nested block = one array (Shape A) */
                if (have_cur && cur.len > 0) {
                    raw_array_t item;
                    char *nm;
                    nm = (group_name != NULL) ? dup_str(group_name) : NULL;
                    if (group_name != NULL && nm == NULL) {
                        fprintf(stderr, "parser: out of memory\n");
                        goto fail;
                    }
                    item.data = cur.data;
                    item.len  = cur.len;
                    item.name = nm;
                    item.group = group_seq;
                    item.sub  = nested_idx++;
                    item.nested = 1;
                    cur.data = NULL; cur.len = 0; cur.cap = 0;
                    have_cur = 0;
                    if (arrays_push(&arrays, &arrays_n, &arrays_cap, item) != 0) {
                        fprintf(stderr, "parser: out of memory\n");
                        goto fail;
                    }
                } else {
                    /* empty nested block - skip but warn */
                    fprintf(stderr, "parser: warning - skipped empty nested block\n");
                }
            }
            if (depth > 0) depth--;
            p++;
            continue;
        }

        /* default: skip one char */
        p++;
    }

    free(cur.data);
    free(src);

    if (arrays_n == 0) {
        fprintf(stderr, "parser: no hex arrays found in '%s'\n", filename);
        return -1;
    }

    *out_arrays = arrays;
    *out_count  = arrays_n;
    return 0;

fail:
    free(cur.data);
    free(src);
    for (i = 0; i < arrays_n; i++) {
        free(arrays[i].data);
        free(arrays[i].name);
    }
    free(arrays);
    return -1;
}

void parser_free(raw_array_t *arrays, unsigned int count)
{
    unsigned int i;
    if (!arrays) return;
    for (i = 0; i < count; i++) {
        free(arrays[i].data);
        free(arrays[i].name);
    }
    free(arrays);
}
