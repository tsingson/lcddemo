#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum {
    MODE_CENTER = 0,
    MODE_TOP_CENTER = 1,
    MODE_BOTTOM_ONLY = 2,
    MODE_FIT_LONG_EDGE = 3,
} crop_mode_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

typedef struct {
    int w;
    int h;
    uint8_t *rgb;
} image_t;

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <input_image> <output_c> <out_w> <out_h> <mode> [symbol]\n\n"
            "Supported input formats: png / jpeg\n\n"
            "Modes:\n"
            "  center       / 0 : center crop\n"
            "  top_center   / 1 : keep top-center, crop sides and/or bottom\n"
            "  bottom_only  / 2 : only crop bottom (fails if source is wider than target ratio)\n"
            "  fit_long     / 3 : no crop, fit long edge, letterbox fill with black\n\n"
            "Example:\n"
            "  %s 2.jpeg src/image_2_240x320_rgb565.c 240 320 center src_image_2_240x320_rgb565\n",
            prog,
            prog);
}

static int parse_mode(const char *s, crop_mode_t *mode)
{
    if (strcmp(s, "0") == 0 || strcmp(s, "center") == 0) {
        *mode = MODE_CENTER;
        return 0;
    }
    if (strcmp(s, "1") == 0 || strcmp(s, "top_center") == 0) {
        *mode = MODE_TOP_CENTER;
        return 0;
    }
    if (strcmp(s, "2") == 0 || strcmp(s, "bottom_only") == 0) {
        *mode = MODE_BOTTOM_ONLY;
        return 0;
    }
    if (strcmp(s, "3") == 0 || strcmp(s, "fit_long") == 0) {
        *mode = MODE_FIT_LONG_EDGE;
        return 0;
    }
    return -1;
}

static const char *path_ext(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot == NULL || dot[1] == '\0') {
        return NULL;
    }
    return dot + 1;
}

static int str_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static int is_supported_input(const char *path)
{
    const char *ext = path_ext(path);
    if (ext == NULL) {
        return 0;
    }

    if (str_ieq(ext, "png")) {
        return 1;
    }
    if (str_ieq(ext, "jpg") || str_ieq(ext, "jpeg")) {
        return 1;
    }

    return 0;
}

static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int run_command(const char *cmd)
{
    int rc = system(cmd);
    if (rc == -1) {
        return -1;
    }
    if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
        return 0;
    }
    return -1;
}

static int probe_image_size(const char *input, int *w, int *h)
{
    char cmd[4096];
    char line[256];
    FILE *fp;
    int got_w = 0;
    int got_h = 0;

    snprintf(cmd,
             sizeof(cmd),
             "sips -g pixelWidth -g pixelHeight \"%s\"",
             input);

    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *p;
        if ((p = strstr(line, "pixelWidth:")) != NULL) {
            *w = atoi(p + (int)strlen("pixelWidth:"));
            got_w = (*w > 0);
        } else if ((p = strstr(line, "pixelHeight:")) != NULL) {
            *h = atoi(p + (int)strlen("pixelHeight:"));
            got_h = (*h > 0);
        }
    }

    pclose(fp);

    if (!got_w || !got_h) {
        return -1;
    }

    return 0;
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int load_bmp_rgb(const char *path, image_t *img)
{
    FILE *fp = fopen(path, "rb");
    uint8_t fh[14];
    uint8_t ih[40];
    uint32_t off_bits;
    int32_t bw, bh;
    uint16_t bpp;
    uint32_t compression;
    int top_down = 0;

    if (fp == NULL) {
        return -1;
    }

    if (fread(fh, 1, sizeof(fh), fp) != sizeof(fh)) {
        fclose(fp);
        return -1;
    }

    if (fh[0] != 'B' || fh[1] != 'M') {
        fclose(fp);
        return -1;
    }

    off_bits = read_le32(&fh[10]);

    if (fread(ih, 1, sizeof(ih), fp) != sizeof(ih)) {
        fclose(fp);
        return -1;
    }

    if (read_le32(&ih[0]) != 40U) {
        fclose(fp);
        return -1;
    }

    bw = (int32_t)read_le32(&ih[4]);
    bh = (int32_t)read_le32(&ih[8]);
    bpp = read_le16(&ih[14]);
    compression = read_le32(&ih[16]);

    if (bw <= 0 || bh == 0) {
        fclose(fp);
        return -1;
    }

    if (bh < 0) {
        top_down = 1;
        bh = -bh;
    }

    if ((bpp != 24U && bpp != 32U) || compression != 0U) {
        fclose(fp);
        return -1;
    }

    img->w = (int)bw;
    img->h = (int)bh;

    size_t row_src = ((size_t)img->w * (size_t)bpp + 31U) / 32U * 4U;
    size_t row_dst = (size_t)img->w * 3U;
    uint8_t *row = (uint8_t *)malloc(row_src);
    if (row == NULL) {
        fclose(fp);
        return -1;
    }

    size_t sz = (size_t)img->w * (size_t)img->h * 3U;
    img->rgb = (uint8_t *)malloc(sz);
    if (img->rgb == NULL) {
        free(row);
        fclose(fp);
        return -1;
    }

    if (fseek(fp, (long)off_bits, SEEK_SET) != 0) {
        free(img->rgb);
        img->rgb = NULL;
        free(row);
        fclose(fp);
        return -1;
    }

    for (int y = 0; y < img->h; ++y) {
        int dst_y = top_down ? y : (img->h - 1 - y);
        uint8_t *dst = img->rgb + (size_t)dst_y * row_dst;

        if (fread(row, 1, row_src, fp) != row_src) {
            free(img->rgb);
            img->rgb = NULL;
            free(row);
            fclose(fp);
            return -1;
        }

        if (bpp == 24U) {
            for (int x = 0; x < img->w; ++x) {
                const uint8_t *s = row + (size_t)x * 3U;
                uint8_t *d = dst + (size_t)x * 3U;
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
            }
        } else {
            for (int x = 0; x < img->w; ++x) {
                const uint8_t *s = row + (size_t)x * 4U;
                uint8_t *d = dst + (size_t)x * 3U;
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
            }
        }
    }

    free(row);

    fclose(fp);
    return 0;
}

static void free_image(image_t *img)
{
    free(img->rgb);
    img->rgb = NULL;
    img->w = 0;
    img->h = 0;
}

static rect_t calc_crop_rect(int sw, int sh, int dw, int dh, crop_mode_t mode, int *err)
{
    rect_t r = {0, 0, sw, sh};
    const double src_ar = (double)sw / (double)sh;
    const double dst_ar = (double)dw / (double)dh;

    *err = 0;

    if (mode == MODE_FIT_LONG_EDGE) {
        return r;
    }

    if (mode == MODE_BOTTOM_ONLY) {
        if (src_ar > dst_ar) {
            *err = 1;
            return r;
        }
        r.w = sw;
        r.h = (int)llround((double)sw / dst_ar);
        if (r.h > sh) {
            r.h = sh;
        }
        r.x = 0;
        r.y = 0;
        return r;
    }

    if (src_ar > dst_ar) {
        r.h = sh;
        r.w = (int)llround((double)sh * dst_ar);
        if (r.w > sw) {
            r.w = sw;
        }
        r.x = (sw - r.w) / 2;
        r.y = 0;
    } else {
        r.w = sw;
        r.h = (int)llround((double)sw / dst_ar);
        if (r.h > sh) {
            r.h = sh;
        }
        r.x = 0;
        r.y = (mode == MODE_TOP_CENTER) ? 0 : (sh - r.h) / 2;
    }

    return r;
}

static void sanitize_symbol(const char *src, char *dst, size_t n)
{
    size_t j = 0;
    if (n == 0) {
        return;
    }

    for (size_t i = 0; src[i] != '\0' && j + 1 < n; ++i) {
        char c = src[i];
        if (isalnum((unsigned char)c) || c == '_') {
            dst[j++] = c;
        } else {
            dst[j++] = '_';
        }
    }
    if (j == 0 || isdigit((unsigned char)dst[0])) {
        if (j + 2 < n) {
            memmove(dst + 1, dst, j);
            dst[0] = 'i';
            j += 1;
        }
    }
    dst[j] = '\0';
}

static uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t rr = (uint16_t)(r >> 3);
    uint16_t gg = (uint16_t)(g >> 2);
    uint16_t bb = (uint16_t)(b >> 3);
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

static void sample_bilinear(const image_t *src, double sx, double sy, uint8_t *r, uint8_t *g, uint8_t *b)
{
    int x0 = (int)floor(sx);
    int y0 = (int)floor(sy);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    double fx = sx - (double)x0;
    double fy = sy - (double)y0;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= src->w) x1 = src->w - 1;
    if (y1 >= src->h) y1 = src->h - 1;

    size_t i00 = ((size_t)y0 * (size_t)src->w + (size_t)x0) * 3U;
    size_t i10 = ((size_t)y0 * (size_t)src->w + (size_t)x1) * 3U;
    size_t i01 = ((size_t)y1 * (size_t)src->w + (size_t)x0) * 3U;
    size_t i11 = ((size_t)y1 * (size_t)src->w + (size_t)x1) * 3U;

    for (int c = 0; c < 3; ++c) {
        double v00 = src->rgb[i00 + (size_t)c];
        double v10 = src->rgb[i10 + (size_t)c];
        double v01 = src->rgb[i01 + (size_t)c];
        double v11 = src->rgb[i11 + (size_t)c];

        double v0 = v00 + (v10 - v00) * fx;
        double v1 = v01 + (v11 - v01) * fx;
        double vv = v0 + (v1 - v0) * fy;

        if (vv < 0.0) vv = 0.0;
        if (vv > 255.0) vv = 255.0;

        if (c == 0) *r = (uint8_t)llround(vv);
        if (c == 1) *g = (uint8_t)llround(vv);
        if (c == 2) *b = (uint8_t)llround(vv);
    }
}

static int write_c_array(const char *out_c,
                         const uint8_t *buf,
                         size_t len,
                         const char *symbol,
                         int out_w,
                         int out_h,
                         crop_mode_t mode)
{
    FILE *fp = fopen(out_c, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp,
            "/* Auto-generated by tools/img_trans.c */\n"
            "/* size: %dx%d, mode: %d */\n"
            "const unsigned char %s[] = {\n",
            out_w,
            out_h,
            (int)mode,
            symbol);

    for (size_t i = 0; i < len; ++i) {
        if ((i % 12U) == 0U) {
            fputs("  ", fp);
        }
        fprintf(fp, "0x%02x", buf[i]);
        if (i + 1U < len) {
            fputs(", ", fp);
        }
        if ((i % 12U) == 11U) {
            fputc('\n', fp);
        }
    }
    if ((len % 12U) != 0U) {
        fputc('\n', fp);
    }

    fprintf(fp, "};\n\nconst unsigned int %s_len = %zu;\n", symbol, len);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_c;
    const char *mode_str;
    const char *symbol_arg = NULL;
    crop_mode_t mode;
    int out_w, out_h;
    char symbol[256];
    char tmp_bmp[1024];
    char cmd[4096];
    image_t src = {0, 0, NULL};
    uint8_t *out_bytes = NULL;

    if (argc < 6 || argc > 7) {
        print_usage(argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_c = argv[2];
    out_w = atoi(argv[3]);
    out_h = atoi(argv[4]);
    mode_str = argv[5];
    if (argc == 7) {
        symbol_arg = argv[6];
    }

    if (out_w <= 0 || out_h <= 0) {
        fprintf(stderr, "Invalid output size: %dx%d\n", out_w, out_h);
        return 1;
    }

    if (parse_mode(mode_str, &mode) != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode_str);
        print_usage(argv[0]);
        return 1;
    }

    if (!file_exists(in_path)) {
        fprintf(stderr, "Input not found: %s\n", in_path);
        return 1;
    }

    if (symbol_arg != NULL) {
        sanitize_symbol(symbol_arg, symbol, sizeof(symbol));
    } else {
        const char *base = strrchr(out_c, '/');
        base = (base == NULL) ? out_c : (base + 1);
        sanitize_symbol(base, symbol, sizeof(symbol));
        char *dot = strrchr(symbol, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
    }

    snprintf(tmp_bmp, sizeof(tmp_bmp), "tools/.img_trans_XXXXXX.bmp");

    int fd = mkstemps(tmp_bmp, 4);
    if (fd < 0) {
        perror("mkstemps");
        return 1;
    }
    close(fd);
    unlink(tmp_bmp);

    if (!is_supported_input(in_path)) {
        fprintf(stderr, "Unsupported input format. Only png/jpeg are accepted.\n");
        unlink(tmp_bmp);
        return 1;
    }

    if (probe_image_size(in_path, &src.w, &src.h) != 0) {
        fprintf(stderr, "Failed to probe image size via sips.\n");
        unlink(tmp_bmp);
        return 1;
    }

    snprintf(cmd,
             sizeof(cmd),
             "sips -s format bmp \"%s\" --out \"%s\" >/dev/null",
             in_path,
             tmp_bmp);

    if (run_command(cmd) != 0) {
        fprintf(stderr, "Failed to convert image to BMP via sips.\n");
        unlink(tmp_bmp);
        return 1;
    }

    if (load_bmp_rgb(tmp_bmp, &src) != 0) {
        fprintf(stderr, "Failed to load decoded BMP data: %s\n", tmp_bmp);
        unlink(tmp_bmp);
        return 1;
    }
    unlink(tmp_bmp);

    out_bytes = (uint8_t *)malloc((size_t)out_w * (size_t)out_h * 2U);
    if (out_bytes == NULL) {
        fprintf(stderr, "Out of memory\n");
        free_image(&src);
        return 1;
    }

    if (mode == MODE_FIT_LONG_EDGE) {
        const double scale = fmin((double)out_w / (double)src.w, (double)out_h / (double)src.h);
        const int scaled_w = (int)llround((double)src.w * scale);
        const int scaled_h = (int)llround((double)src.h * scale);
        const int ox = (out_w - scaled_w) / 2;
        const int oy = (out_h - scaled_h) / 2;

        for (int y = 0; y < out_h; ++y) {
            for (int x = 0; x < out_w; ++x) {
                uint16_t rgb565;
                if (x < ox || x >= ox + scaled_w || y < oy || y >= oy + scaled_h) {
                    rgb565 = 0x0000U;
                } else {
                    double sx = ((double)(x - ox) + 0.5) * (double)src.w / (double)scaled_w - 0.5;
                    double sy = ((double)(y - oy) + 0.5) * (double)src.h / (double)scaled_h - 0.5;
                    uint8_t r, g, b;
                    sample_bilinear(&src, sx, sy, &r, &g, &b);
                    rgb565 = rgb888_to_rgb565(r, g, b);
                }
                size_t idx = ((size_t)y * (size_t)out_w + (size_t)x) * 2U;
                out_bytes[idx] = (uint8_t)(rgb565 >> 8);
                out_bytes[idx + 1U] = (uint8_t)(rgb565 & 0xFF);
            }
        }
    } else {
        int err = 0;
        rect_t crop = calc_crop_rect(src.w, src.h, out_w, out_h, mode, &err);
        if (err != 0) {
            fprintf(stderr,
                    "mode=bottom_only cannot satisfy target ratio when source is wider than target.\\n"
                    "source=%dx%d target=%dx%d\\n",
                    src.w,
                    src.h,
                    out_w,
                    out_h);
            free(out_bytes);
            free_image(&src);
            return 1;
        }

        for (int y = 0; y < out_h; ++y) {
            for (int x = 0; x < out_w; ++x) {
                double sx = (double)crop.x + ((double)x + 0.5) * (double)crop.w / (double)out_w - 0.5;
                double sy = (double)crop.y + ((double)y + 0.5) * (double)crop.h / (double)out_h - 0.5;
                uint8_t r, g, b;
                uint16_t rgb565;
                size_t idx;

                sample_bilinear(&src, sx, sy, &r, &g, &b);
                rgb565 = rgb888_to_rgb565(r, g, b);
                idx = ((size_t)y * (size_t)out_w + (size_t)x) * 2U;
                out_bytes[idx] = (uint8_t)(rgb565 >> 8);
                out_bytes[idx + 1U] = (uint8_t)(rgb565 & 0xFF);
            }
        }
    }

    if (write_c_array(out_c,
                      out_bytes,
                      (size_t)out_w * (size_t)out_h * 2U,
                      symbol,
                      out_w,
                      out_h,
                      mode) != 0) {
        fprintf(stderr, "Failed to write: %s\n", out_c);
        free(out_bytes);
        free_image(&src);
        return 1;
    }

    free(out_bytes);
    free_image(&src);

    printf("Generated %s (symbol=%s, bytes=%u)\n",
           out_c,
           symbol,
           (unsigned int)((size_t)out_w * (size_t)out_h * 2U));
    return 0;
}
