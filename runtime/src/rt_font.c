/* Polices hôte pour le texte GDI. Le fichier de police est choisi par fontconfig (fc-match) ; à défaut, Liberation
 * Sans (métriquement compatible avec Arial). Le rendu des glyphes utilise stb_truetype (domaine public, v1.26). */

#include "rt_font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_MAX 16

typedef struct {
    char key[160];
    unsigned char *data;
    stbtt_fontinfo info;
    int ok;
} font_slot;

static font_slot fonts[FONT_MAX];
static int font_count;

static const char *fallback_paths[] = {
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
    NULL,
};

/* Chemin du fichier de police pour « face » : fc-match, seulement avec des caractères sûrs. */
static int find_font_file(const char *face, int bold, int italic, char *path, size_t cap)
{
    char pattern[200];
    size_t n = 0;
    for (const char *p = face; *p && n + 1 < sizeof pattern - 32; p++) {
        if (isalnum((unsigned char)*p) || *p == ' ' || *p == '-') pattern[n++] = *p;
    }
    if (n == 0) {
        memcpy(pattern, "Arial", 6);
        n = 5;
    }
    pattern[n] = 0;
    if (bold) strcat(pattern, ":bold");
    if (italic) strcat(pattern, ":italic");
    char cmd[320];
    snprintf(cmd, sizeof cmd, "fc-match -f '%%{file}' '%s' 2>/dev/null", pattern);
    FILE *f = popen(cmd, "r");
    if (!f) return 0;
    size_t got = fread(path, 1, cap - 1, f);
    pclose(f);
    path[got] = 0;
    return got > 0;
}

static int load_file(const char *path, unsigned char **data, long *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *data = malloc((size_t)*size);
    int ok = *data && fread(*data, 1, (size_t)*size, f) == (size_t)*size;
    fclose(f);
    return ok;
}

int rt_font_open(const char *face, int bold, int italic)
{
    char key[160];
    snprintf(key, sizeof key, "%s|%d|%d", face ? face : "", bold ? 1 : 0, italic ? 1 : 0);
    for (int i = 0; i < font_count; i++) {
        if (strcmp(fonts[i].key, key) == 0) return fonts[i].ok ? i : -1;
    }
    if (font_count >= FONT_MAX) return -1;
    font_slot *s = &fonts[font_count];
    snprintf(s->key, sizeof s->key, "%s", key);
    char path[1024];
    long size = 0;
    int loaded = find_font_file(face ? face : "", bold, italic, path, sizeof path) && load_file(path, &s->data, &size);
    for (int k = 0; !loaded && fallback_paths[k]; k++) {
        loaded = load_file(fallback_paths[k], &s->data, &size);
    }
    s->ok = loaded && stbtt_InitFont(&s->info, s->data, stbtt_GetFontOffsetForIndex(s->data, 0));
    if (!s->ok) {
        fprintf(stderr, "rt: police introuvable pour « %s » (fc-match et repli)\n", face ? face : "");
        free(s->data);
        s->data = NULL;
    }
    font_count++;
    return s->ok ? font_count - 1 : -1;
}

void rt_font_metrics(int font, float px, int *ascent, int *descent)
{
    float scale = stbtt_ScaleForMappingEmToPixels(&fonts[font].info, px);
    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&fonts[font].info, &asc, &desc, &gap);
    *ascent = (int)(asc * scale + 0.5f);
    *descent = (int)(-desc * scale + 0.5f);
}

float rt_font_advance(int font, float px, unsigned cp)
{
    float scale = stbtt_ScaleForMappingEmToPixels(&fonts[font].info, px);
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&fonts[font].info, (int)cp, &adv, &lsb);
    return adv * scale;
}

unsigned char *rt_font_glyph(int font, float px, unsigned cp, int *w, int *h, int *xoff, int *yoff)
{
    float scale = stbtt_ScaleForMappingEmToPixels(&fonts[font].info, px);
    return stbtt_GetCodepointBitmap(&fonts[font].info, 0, scale, (int)cp, w, h, xoff, yoff);
}

void rt_font_glyph_free(unsigned char *bitmap)
{
    stbtt_FreeBitmap(bitmap, NULL);
}
