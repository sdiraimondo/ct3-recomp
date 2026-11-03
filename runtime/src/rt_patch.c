/* Réglages du jeu lus dans Config.ini (sections [Patches] et [Game]) : saut des logos, format d'image et champ de
 * vision. Adresses et comportements d'après taxipatch (faits seulement, aucun code copié) et l'analyse de
 * l'exécutable (docs/TAXIPATCH_PLAN.md). Un réglage absent ne modifie rien : le jeu garde sa valeur d'origine.
 *
 * Fichier : CT3_CONFIG, sinon Config.ini dans le répertoire courant. Syntaxe : clé=valeur, commentaires ; ou #.
 *   [Patches] SkipIntro=1|0       saut des logos au démarrage (défaut : 1)
 *   [Game]    Aspect=16:9|4:3     format de la perspective de la caméra
 *   [Game]    Fov=80              champ de vision horizontal en degrés (défaut : 60 en 4:3, 80 en 16:9) */

#include "rt.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <strings.h>

/* Adresses (mémoire invitée) : compteur de la séquence des logos, globales de la caméra lues à chaque image. */
#define PATCH_LOGO_COUNTER 0x00717884u
#define PATCH_LOGO_DONE 1024u
#define PATCH_CAM_FOV 0x0071C3C8u
#define PATCH_CAM_ASPECT 0x0071C420u

static int patch_skip_intro = 1;
static float patch_aspect;   /* 0 : ne pas écrire */
static float patch_fov;      /* 0 : ne pas écrire */
static int patch_ui_texcoord = 1;
static float patch_map_scale = 1.0f;   /* 1.0 : valeurs d'origine */

static uint32_t rd32_(uint32_t a)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + a, 4);
    return v;
}

static void wr32_(uint32_t a, uint32_t v)
{
    memcpy(rt_mem_base + a, &v, 4);
}

static float rdf_(uint32_t a)
{
    float f;
    memcpy(&f, rt_mem_base + a, 4);
    return f;
}

static void wrf_(uint32_t a, float f)
{
    memcpy(rt_mem_base + a, &f, 4);
}

static void patch_trim(char *s)
{
    char *end;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    memmove(s, p, strlen(p) + 1);
    for (char *q = s; *q; q++) {
        if (*q == ';' || *q == '#') {
            *q = '\0';
            break;
        }
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
}

static void patch_set(const char *section, const char *key, const char *value)
{
    if (!strcasecmp(section, "patches") && !strcasecmp(key, "SkipIntro")) {
        patch_skip_intro = atoi(value) != 0;
    } else if (!strcasecmp(section, "patches") && !strcasecmp(key, "UiTexcoord")) {
        patch_ui_texcoord = atoi(value) != 0;
    } else if (!strcasecmp(section, "distance") && !strcasecmp(key, "Map")) {
        float f = strtof(value, NULL);
        if (f > 0.0f && f < 16.0f) {
            patch_map_scale = f;
        } else {
            fprintf(stderr, "rt: Config.ini : Map=%s ignoré (facteur entre 0 et 16)\n", value);
        }
    } else if (!strcasecmp(section, "game") && !strcasecmp(key, "Aspect")) {
        int w = 0, h = 0;
        if (sscanf(value, "%d:%d", &w, &h) == 2 && w > 0 && h > 0) {
            patch_aspect = (float)w / (float)h;
        } else {
            fprintf(stderr, "rt: Config.ini : Aspect=%s ignoré (attendu W:H, par ex. 16:9)\n", value);
        }
    } else if (!strcasecmp(section, "game") && !strcasecmp(key, "Fov")) {
        float f = strtof(value, NULL);
        if (f > 1.0f && f < 179.0f) {
            patch_fov = f;
        } else {
            fprintf(stderr, "rt: Config.ini : Fov=%s ignoré (degrés entre 1 et 179)\n", value);
        }
    }
}

void rt_patch_load(void)
{
    const char *path = getenv("CT3_CONFIG");
    FILE *f;
    char line[512];
    char section[64] = "";
    int fov_set = 0;

    if (!path) path = "Config.ini";
    f = fopen(path, "rb");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq;
        patch_trim(line);
        if (line[0] == '\0') continue;
        if (line[0] == '[') {
            char *close = strchr(line, ']');
            if (close) *close = '\0';
            snprintf(section, sizeof section, "%s", line + 1);
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        {
            char *key = line, *value = eq + 1;
            patch_trim(key);
            patch_trim(value);
            if (!strcasecmp(section, "game") && !strcasecmp(key, "Fov")) fov_set = 1;
            patch_set(section, key, value);
        }
    }
    fclose(f);
    if (patch_aspect > 0.0f && !fov_set) {
        /* Sans Fov explicite : 60° en 4:3 (valeur d'origine), 80° en 16:9 (valeur de CT3Tweaks). */
        patch_fov = patch_aspect < 1.5f ? 60.0f : 80.0f;
    }
    if (getenv("CT3_PATCH_LOG")) {
        fprintf(stderr, "rt: réglages : saut des logos=%d, format=%.4f, champ de vision=%.1f\n", patch_skip_intro,
                (double)patch_aspect, (double)patch_fov);
    }
}

/* Crochet de la séquence des logos (avant la traduction de FUN_0042E160). */
void rt_patch_before_logo(cpu_t *c)
{
    (void)c;
    if (patch_skip_intro) {
        uint32_t v = PATCH_LOGO_DONE;
        memcpy(rt_mem_base + PATCH_LOGO_COUNTER, &v, 4);
    }
}

/* Appelé à chaque présentation : la caméra relit ses globales à chaque image. */
void rt_patch_frame(void)
{
    if (patch_aspect > 0.0f) memcpy(rt_mem_base + PATCH_CAM_ASPECT, &patch_aspect, 4);
    if (patch_fov > 0.0f) memcpy(rt_mem_base + PATCH_CAM_FOV, &patch_fov, 4);
}

int rt_patch_ui_texcoord_enabled(void)
{
    return patch_ui_texcoord;
}

/* Crochet après Map::UpdateDrawDistance (FUN_00447ED0) : la routine réécrit ses valeurs à chaque appel, on les
 * multiplie ensuite (même logique que taxipatch : maillage, nombre de maillages, LOD, plan lointain de la caméra). */
void rt_patch_after_map(cpu_t *c)
{
    float scale = patch_map_scale;
    float radius, lod, far_clip, mc_f;
    int32_t count;
    (void)c;
    if (scale == 1.0f) return;
    radius = rdf_(0x007239D4u) * scale;
    wrf_(0x007239D4u, radius);
    mc_f = (float)rd32_(0x007239D8u) * scale;
    count = (int32_t)ceilf(mc_f);
    if (count < 1) count = 1;
    if (count > 200) count = 200;
    wr32_(0x007239D8u, (uint32_t)count);
    lod = rdf_(0x00724DF0u);
    if (lod > 0.0f) wrf_(0x00724DF0u, lod * scale);
    far_clip = rdf_(0x0071C430u);
    if (far_clip < 10000.0f) far_clip = 10000.0f;
    if (far_clip < radius * 1.1f) far_clip = radius * 1.1f;
    wrf_(0x0071C430u, far_clip);
}
