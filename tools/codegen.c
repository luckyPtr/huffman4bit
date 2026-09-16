/**
 * @file codegen.c
 * @brief Emit a .c/.h pair from the compressed arrays and Huffman tables.
 *
 * Output naming derived from the (sanitised) basename:
 *   <basename>.h     - declarations
 *   <basename>.c     - tables + data arrays
 *
 * Variable / macro prefixes follow the pattern h4b_<basename>_*  /
 * H4B_<BASENAME>_*.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "codegen.h"
#include "huffman4bit.h"

/* ------------------------------------------------------------------ */
/*  Helpers to build identifier strings                               */
/* ------------------------------------------------------------------ */

char *codegen_sanitize_basename(const char *raw)
{
    size_t n;
    char *out;
    size_t i;
    int needs_prefix;

    if (!raw || !*raw) raw = "out";
    n = strlen(raw);
    needs_prefix = (isdigit((unsigned char)raw[0])) ? 1 : 0;

    out = (char *)malloc(n + 1 + needs_prefix);
    if (!out) return NULL;

    if (needs_prefix) {
        out[0] = '_';
    }
    for (i = 0; i < n; i++) {
        char c = raw[i];
        out[i + needs_prefix] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
    }
    out[n + needs_prefix] = '\0';
    return out;
}

int codegen_split_output(const char *raw, char **outdir, char **basename)
{
    const char *sep  = strrchr(raw, '/');
    const char *sep2 = strrchr(raw, '\\');
    const char *name;
    size_t dirlen;
    char *d, *b;

    if (sep2 > sep) sep = sep2;
    name = sep ? sep + 1 : raw;

    if (!*name) {
        fprintf(stderr, "error: -o '%s' has no file name after the directory part\n", raw);
        return -1;
    }

    b = codegen_sanitize_basename(name);
    if (!b) {
        fprintf(stderr, "error: out of memory\n");
        return -1;
    }

    if (sep) {
        dirlen = (size_t)(sep - raw);
        d = (char *)malloc(dirlen + 1);
        if (!d) {
            fprintf(stderr, "error: out of memory\n");
            free(b);
            return -1;
        }
        memcpy(d, raw, dirlen);
        d[dirlen] = '\0';
        *outdir = d;
    } else {
        *outdir = NULL;
    }
    *basename = b;
    return 0;
}

/* Build "h4b_<base>_<suffix>" - caller frees. */
static char *mkname(const char *base, const char *suffix)
{
    size_t a = strlen(base), b = strlen(suffix);
    char *s = (char *)malloc(a + b + 6);   /* "h4b_" + "_" + nul */
    if (!s) return NULL;
    sprintf(s, "h4b_%s_%s", base, suffix);
    return s;
}

/* Build "H4B_<BASE>_<SUFFIX>" (uppercase) - caller frees. */
static char *mkmacro(const char *base, const char *suffix)
{
    size_t a = strlen(base), b = strlen(suffix);
    char *s = (char *)malloc(a + b + 6);
    size_t i;
    if (!s) return NULL;
    /* construct lower first then uppercase in-place */
    sprintf(s, "h4b_%s_%s", base, suffix);
    for (i = 0; s[i]; i++) {
        if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 'a' + 'A');
    }
    return s;
}

/* ------------------------------------------------------------------ */
/*  Symbol-name model: original array names + grouping                 */
/*                                                                    */
/*  1D input  FOO[] = {...}     -> public blob "FOO" (sized extern,    */
/*                               consumers use sizeof(FOO))            */
/*  2D input  FOO[][N] = {...}  -> static blobs FOO_0..FOO_{n-1} plus  */
/*          const unsigned char *const FOO[n]  (pointer table,         */
/*          original name) and const unsigned int FOO_LEN[n]           */
/*  Tables / macros keep the basename-derived h4b_<basename>_* names.  */
/* ------------------------------------------------------------------ */

static int valid_ident(const char *s)
{
    size_t i;
    if (s == NULL) return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (i = 1; s[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_')) return 0;
    }
    return 1;
}

/* bounded strdup (C89 has no strdup) */
static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

typedef struct {
    unsigned int start;     /* index of the group's first array              */
    unsigned int count;     /* arrays in the group                           */
    char *ptr_name;         /* pointer-table symbol, NULL for 1D groups      */
    char *len_name;         /* length-table symbol, NULL for 1D groups       */
} group_desc_t;

typedef struct {
    char *v_tbl;                        /* h4b_<basename>_tables             */
    char *m_k;                          /* H4B_<BASENAME>_CLUSTER_COUNT      */
    char *m_guard;                      /* include-guard macro               */
    char *fallback_prefix;              /* h4b_<basename>_data (unnamed)     */
    char **blobs;                       /* [blob_count] blob symbol names    */
    unsigned int blob_count;
    group_desc_t *groups;
    unsigned int group_count;
} name_model_t;

static void name_model_free(name_model_t *m)
{
    unsigned int i;
    if (m->blobs) {
        for (i = 0; i < m->blob_count; i++) free(m->blobs[i]);
        free(m->blobs);
    }
    if (m->groups) {
        for (i = 0; i < m->group_count; i++) {
            free(m->groups[i].ptr_name);
            free(m->groups[i].len_name);
        }
        free(m->groups);
    }
    free(m->v_tbl); free(m->m_k); free(m->m_guard);
    free(m->fallback_prefix);
}

/* Resolve every emitted symbol name; warn + use the basename-derived
 * fallback for arrays whose declarator could not be captured. */
static int build_name_model(const codegen_ctx_t *ctx, name_model_t *m)
{
    unsigned int i, g;
    char buf[1024];

    m->v_tbl = NULL; m->m_k = NULL; m->m_guard = NULL;
    m->fallback_prefix = NULL; m->blobs = NULL; m->groups = NULL;
    m->blob_count = ctx->data_count; m->group_count = 0;

    m->m_k     = mkmacro(ctx->basename, "CLUSTER_COUNT");
    m->m_guard = mkmacro(ctx->basename, "H");
    m->v_tbl   = mkname(ctx->basename, "tables");
    m->fallback_prefix = mkname(ctx->basename, "data");
    if (!m->m_k || !m->m_guard || !m->v_tbl || !m->fallback_prefix) {
        fprintf(stderr, "codegen: out of memory\n");
        return -1;
    }

    /* count contiguous groups (arrays of one "= {...}" are adjacent) */
    for (i = 0; i < ctx->data_count; i++) {
        if (i == 0 || ctx->arrays[i].group != ctx->arrays[i - 1U].group) {
            m->group_count++;
        }
    }

    m->blobs = (char **)calloc(ctx->data_count, sizeof(char *));
    m->groups = (group_desc_t *)calloc(m->group_count, sizeof(group_desc_t));
    if (!m->blobs || !m->groups) {
        fprintf(stderr, "codegen: out of memory\n");
        return -1;
    }

    /* fill group descriptors */
    g = 0;
    for (i = 0; i < ctx->data_count; i++) {
        if (i == 0 || ctx->arrays[i].group != ctx->arrays[i - 1U].group) {
            m->groups[g].start = i;
            m->groups[g].count = 1;
            g++;
        } else {
            m->groups[g - 1U].count++;
        }
    }
    for (g = 0; g < m->group_count; g++) {
        const raw_array_t *first = &ctx->arrays[m->groups[g].start];
        if (first->nested) {
            if (valid_ident(first->name)) {
                sprintf(buf, "%s_LEN", first->name);
                m->groups[g].ptr_name = dup_str(first->name);
                m->groups[g].len_name = dup_str(buf);
            } else {
                fprintf(stderr,
                    "codegen: warning - 2D array group %u has no usable "
                    "declarator name, falling back to %s\n",
                    g, m->fallback_prefix);
                m->groups[g].ptr_name = dup_str(m->fallback_prefix);
                sprintf(buf, "%s_LEN", m->fallback_prefix);
                m->groups[g].len_name = dup_str(buf);
            }
            if (!m->groups[g].ptr_name || !m->groups[g].len_name) {
                fprintf(stderr, "codegen: out of memory\n");
                return -1;
            }
        }
        /* 1D groups keep ptr_name == len_name == NULL */
    }

    /* resolve blob names */
    for (i = 0; i < ctx->data_count; i++) {
        const raw_array_t *a = &ctx->arrays[i];
        if (valid_ident(a->name)) {
            if (a->nested) sprintf(buf, "%s_%u", a->name, a->sub);
            else           sprintf(buf, "%s", a->name);
        } else {
            fprintf(stderr,
                "codegen: warning - array %u has no usable declarator "
                "name, falling back to %s_%u\n",
                i, m->fallback_prefix, i);
            sprintf(buf, "%s_%u", m->fallback_prefix, i);
        }
        m->blobs[i] = dup_str(buf);
        if (!m->blobs[i]) {
            fprintf(stderr, "codegen: out of memory\n");
            return -1;
        }
    }
    return 0;
}

/* Reject any duplicate emitted identifier before a file is opened: original
 * names can clash with each other or with the generated suffixes (_<n>,
 * _len) and the basename-derived table/macro names. */
static int check_symbol_collisions(const name_model_t *m)
{
    const char **all;
    unsigned int total, n = 0, a, b;

    total = 3U + m->blob_count + 2U * m->group_count;
    all = (const char **)malloc(total * sizeof(char *));
    if (!all) {
        fprintf(stderr, "codegen: out of memory\n");
        return -1;
    }
    all[n++] = m->v_tbl; all[n++] = m->m_k; all[n++] = m->m_guard;
    for (a = 0; a < m->blob_count; a++) all[n++] = m->blobs[a];
    for (a = 0; a < m->group_count; a++) {
        if (m->groups[a].ptr_name) {
            all[n++] = m->groups[a].ptr_name;
            all[n++] = m->groups[a].len_name;
        }
    }
    for (a = 0; a < n; a++) {
        for (b = a + 1U; b < n; b++) {
            if (strcmp(all[a], all[b]) == 0) {
                fprintf(stderr,
                    "codegen: symbol '%s' would be emitted more than once; "
                    "rename the conflicting array in the input file\n",
                    all[a]);
                free(all);
                return -1;
            }
        }
    }
    free(all);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Header (.h) writer                                                */
/* ------------------------------------------------------------------ */

static int write_header(const codegen_ctx_t *ctx, const name_model_t *m, const char *path)
{
    FILE *f;
    unsigned int g;

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "codegen: cannot open '%s' for write\n", path);
        return -1;
    }

    fprintf(f,
        "#ifndef __%s__\n"
        "#define __%s__\n"
        "#include \"huffman4bit.h\"\n"
        "\n"
        "#define %s  %u\n"
        "\n"
        "extern const huffman_tree_tbl_t %s[%s];\n",
        m->m_guard, m->m_guard,
        m->m_k,   ctx->cluster_k,
        m->v_tbl, m->m_k);

    for (g = 0; g < m->group_count; g++) {
        if (m->groups[g].ptr_name) {
            /* 2D input array: pointer + length tables under the original name */
            fprintf(f,
                "\nextern const unsigned char *const %s[%u];\n"
                "extern const unsigned int %s[%u];\n",
                m->groups[g].ptr_name, m->groups[g].count,
                m->groups[g].len_name, m->groups[g].count);
        } else {
            /* 1D input array: the blob keeps the original name (sized extern,
             * so consumers may use sizeof()) */
            fprintf(f,
                "extern const unsigned char %s[%u];\n",
                m->blobs[m->groups[g].start], ctx->enc_lens[m->groups[g].start]);
        }
    }

    fprintf(f, "\n#endif\n");
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Source (.c) writer                                                */
/* ------------------------------------------------------------------ */

static int write_source(const codegen_ctx_t *ctx, const name_model_t *m, const char *path)
{
    FILE *f;
    unsigned int k, i, j, g;
    char *h_name;

    /* build header file name (basename + ".h") */
    h_name = (char *)malloc(strlen(ctx->basename) + 3);
    if (!h_name) {
        fprintf(stderr, "codegen: out of memory\n");
        return -1;
    }
    sprintf(h_name, "%s.h", ctx->basename);

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "codegen: cannot open '%s' for write\n", path);
        free(h_name);
        return -1;
    }

    fprintf(f,
        "/* Auto-generated by h4b_encoder " H4B_VERSION ". DO NOT EDIT. */\n"
        "#include \"%s\"\n\n", h_name);

    /* ---- tables ---- */
    fprintf(f, "const huffman_tree_tbl_t %s[%s] = {\n", m->v_tbl, m->m_k);
    for (k = 0; k < ctx->cluster_k; k++) {
        fprintf(f, "    { /* cluster %u */\n", k);
        for (i = 0; i < MAX_SYMBOLS - 1U; i++) {
            fprintf(f, "        {0x%02X, 0x%02X}%s\n",
                    ctx->tables[k][i][0], ctx->tables[k][i][1],
                    (i < MAX_SYMBOLS - 2U) ? "," : "");
        }
        fprintf(f, "    }%s\n", (k < ctx->cluster_k - 1U) ? "," : "");
    }
    fprintf(f, "};\n\n");

    /* ---- per-array compressed blobs (1D public, 2D sub-images static) ---- */
    for (i = 0; i < ctx->data_count; i++) {
        fprintf(f, "%s const unsigned char %s[] = {",
                ctx->arrays[i].nested ? "static" : "",
                m->blobs[i]);
        for (j = 0; j < ctx->enc_lens[i]; j++) {
            if ((j % 16) == 0) fprintf(f, "\n    ");
            fprintf(f, "0x%02X", ctx->enc[i][j]);
            if (j < ctx->enc_lens[i] - 1U) fprintf(f, ", ");
        }
        fprintf(f, "\n};\n\n");
    }

    /* ---- per-2D-group pointer + length tables (original names) ---- */
    for (g = 0; g < m->group_count; g++) {
        if (!m->groups[g].ptr_name) continue;
        fprintf(f, "const unsigned char *const %s[%u] = {\n",
                m->groups[g].ptr_name, m->groups[g].count);
        for (i = 0; i < m->groups[g].count; i++) {
            fprintf(f, "    %s%s\n", m->blobs[m->groups[g].start + i],
                    (i < m->groups[g].count - 1U) ? "," : "");
        }
        fprintf(f, "};\n\n");
        fprintf(f, "const unsigned int %s[%u] = {\n",
                m->groups[g].len_name, m->groups[g].count);
        for (i = 0; i < m->groups[g].count; i++) {
            fprintf(f, "    sizeof(%s)%s\n", m->blobs[m->groups[g].start + i],
                    (i < m->groups[g].count - 1U) ? "," : "");
        }
        fprintf(f, "};\n\n");
    }

    fclose(f);
    free(h_name);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public entry                                                      */
/* ------------------------------------------------------------------ */

int codegen_write(const codegen_ctx_t *ctx)
{
    char *h_path, *c_path;
    size_t n = strlen(ctx->basename);
    size_t d = (ctx->outdir && *ctx->outdir) ? strlen(ctx->outdir) + 1 : 0;
    name_model_t model;
    int rc;

    if (build_name_model(ctx, &model) != 0) {
        name_model_free(&model);
        return -1;
    }
    if (check_symbol_collisions(&model) != 0) {
        name_model_free(&model);
        return -1;
    }

    h_path = (char *)malloc(d + n + 3);
    c_path = (char *)malloc(d + n + 3);
    if (!h_path || !c_path) {
        free(h_path); free(c_path);
        name_model_free(&model);
        return -1;
    }
    if (d) {
        sprintf(h_path, "%s/%s.h", ctx->outdir, ctx->basename);
        sprintf(c_path, "%s/%s.c", ctx->outdir, ctx->basename);
    } else {
        sprintf(h_path, "%s.h", ctx->basename);
        sprintf(c_path, "%s.c", ctx->basename);
    }

    rc = write_header(ctx, &model, h_path);
    if (rc == 0) rc = write_source(ctx, &model, c_path);

    free(h_path);
    free(c_path);
    name_model_free(&model);
    return rc;
}
