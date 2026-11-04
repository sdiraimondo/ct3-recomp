/* Charge l'image invitée depuis le pack de données CT3 (lot 1).
 *
 * Le runtime ne lit plus CT3.exe : le pack contient les en-têtes PE, .text,
 * .rdata, .data, .rsrc et la table d'imports. Le code x86 reste traduit en C ;
 * .text n'est copiée que comme données (tables de sauts) et n'est jamais
 * exécutée. Le BSS (vsize > raw_size) est zéro-rempli ; les sections exclues
 * du pack restent à zéro. */

#include "rt.h"
#include "rt_pe.h"
#include "ct3_image.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int rt_mem_init(void);
int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
void rt_mem_fill(uint32_t addr, uint32_t size, uint8_t value);

#if CT3_IMAGE_BASE != RT_IMAGE_BASE
#error "le pack et le runtime ne visent pas la même base d'image"
#endif

static int path_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int path_exists(const char *path)
{
    return access(path, R_OK) == 0;
}

static int join_path(char *out, size_t n, const char *dir, const char *name)
{
    int len;
    if (!dir || !*dir || strcmp(dir, ".") == 0) {
        len = snprintf(out, n, "%s", name);
    } else {
        size_t dlen = strlen(dir);
        if (dir[dlen - 1] == '/') {
            len = snprintf(out, n, "%s%s", dir, name);
        } else {
            len = snprintf(out, n, "%s/%s", dir, name);
        }
    }
    return len >= 0 && (size_t)len < n;
}

static void dirname_of(const char *path, char *out, size_t n)
{
    const char *slash;
    if (!path || !*path) {
        snprintf(out, n, ".");
        return;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, n, ".");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len == 0) len = 1; /* racine */
    if (len >= n) len = n - 1;
    memcpy(out, path, len);
    out[len] = 0;
}

/* Essaie un fichier, ou un dossier contenant ct3_data.bin. */
static int resolve_candidate(const char *candidate, char *out, size_t n)
{
    char joined[4096];
    if (!candidate || !*candidate) return 0;
    if (path_is_dir(candidate)) {
        if (!join_path(joined, sizeof joined, candidate, "ct3_data.bin")) return 0;
        if (!path_exists(joined)) return 0;
        snprintf(out, n, "%s", joined);
        return 1;
    }
    if (path_exists(candidate)) {
        snprintf(out, n, "%s", candidate);
        return 1;
    }
    return 0;
}

static int find_pack(const char *pack_path, const char *argv0, char *out, size_t n)
{
    static const char *const relative[] = {
        "ct3_data.bin",
        "pack/ct3_data.bin",
        "../pack/ct3_data.bin",
        "../../pack/ct3_data.bin",
        "../share/ct3/ct3_data.bin",
    };
    const char *env = getenv("CT3_PACK");

    if (pack_path && *pack_path) {
        if (resolve_candidate(pack_path, out, n)) return 1;
        fprintf(stderr, "rt: pack introuvable : %s\n", pack_path);
        return 0;
    }
    if (env && *env) {
        if (resolve_candidate(env, out, n)) return 1;
        fprintf(stderr, "rt: CT3_PACK introuvable : %s\n", env);
        return 0;
    }

    /* Répertoire courant. */
    for (size_t i = 0; i < sizeof relative / sizeof relative[0]; i++) {
        if (path_exists(relative[i])) {
            snprintf(out, n, "%s", relative[i]);
            return 1;
        }
    }

    /* À côté du binaire et dans les dossiers voisins. */
    if (argv0 && *argv0) {
        char dir[4096];
        char candidate[4096];
        dirname_of(argv0, dir, sizeof dir);
        for (size_t i = 0; i < sizeof relative / sizeof relative[0]; i++) {
            if (!join_path(candidate, sizeof candidate, dir, relative[i])) continue;
            if (path_exists(candidate)) {
                snprintf(out, n, "%s", candidate);
                return 1;
            }
        }
    }

    fprintf(stderr,
            "rt: pack de données CT3 introuvable.\n"
            "    Générez-le avec : cmake --build build --target ct3pack\n"
            "    ou définissez CT3_PACK=/chemin/vers/build/pack\n");
    return 0;
}

static int read_file(const char *path, uint8_t **out, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    long end;
    uint8_t *data;

    if (!f) {
        fprintf(stderr, "rt: %s : %s\n", path, strerror(errno));
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (end = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "rt: %s : lecture impossible\n", path);
        fclose(f);
        return -1;
    }
    data = malloc(end > 0 ? (size_t)end : 1);
    if (!data) {
        fprintf(stderr, "rt: %s : mémoire insuffisante\n", path);
        fclose(f);
        return -1;
    }
    if (fread(data, 1, (size_t)end, f) != (size_t)end) {
        fprintf(stderr, "rt: %s : lecture incomplète\n", path);
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);
    *out = data;
    *out_size = (size_t)end;
    return 0;
}

static void free_image(rt_pe_image *img)
{
    if (!img) return;
    for (unsigned i = 0; i < RT_PE_MAX_IMPORTS; i++) free(img->import_names[i]);
    free(img);
}

static int section_contains(const ct3_pack_section *sec, uint32_t address, uint32_t size)
{
    uint64_t end = (uint64_t)address + size;
    return address >= sec->va && end <= (uint64_t)sec->va + sec->vsize;
}

rt_pe_image *rt_pe_load(const char *pack_path, const char *argv0)
{
    char path[4096];
    uint8_t *data = NULL;
    size_t size = 0;
    rt_pe_image *img = NULL;

    if (!find_pack(pack_path, argv0, path, sizeof path)) return NULL;
    if (read_file(path, &data, &size) != 0) return NULL;

    if (size != CT3_IMAGE_DATA_SIZE) {
        fprintf(stderr, "rt: %s : taille 0x%zx, attendu 0x%x\n", path, size,
                (unsigned)CT3_IMAGE_DATA_SIZE);
        free(data);
        return NULL;
    }
    if (size < 0x40 || data[0] != 'M' || data[1] != 'Z') {
        fprintf(stderr, "rt: %s : en-tête PE absent du pack\n", path);
        free(data);
        return NULL;
    }

    if (rt_mem_init() != 0) {
        free(data);
        return NULL;
    }
    if (rt_mem_protect(CT3_IMAGE_BASE, CT3_IMAGE_SIZE, 3) != 0) {
        free(data);
        return NULL;
    }
    rt_mem_fill(CT3_IMAGE_BASE, CT3_IMAGE_SIZE, 0);

    for (unsigned i = 0; i < ct3_pack_section_count; i++) {
        const ct3_pack_section *sec = &ct3_pack_sections[i];
        uint64_t end = (uint64_t)sec->va + (sec->vsize ? sec->vsize : sec->raw_size);
        if (end > (uint64_t)CT3_IMAGE_BASE + CT3_IMAGE_SIZE || sec->va < CT3_IMAGE_BASE) {
            fprintf(stderr, "rt: pack : section %s hors image\n", sec->name);
            free(data);
            return NULL;
        }
        if (!sec->present || sec->raw_size == 0) continue;
        if ((uint64_t)sec->offset + sec->raw_size > size) {
            fprintf(stderr, "rt: pack : section %s hors du fichier\n", sec->name);
            free(data);
            return NULL;
        }
        if (sec->vsize && sec->raw_size > sec->vsize) {
            fprintf(stderr, "rt: pack : section %s plus grande que sa taille virtuelle\n", sec->name);
            free(data);
            return NULL;
        }
        memcpy(rt_mem_base + sec->va, data + sec->offset, sec->raw_size);
    }

    img = calloc(1, sizeof(*img));
    if (!img) {
        fprintf(stderr, "rt: pack : mémoire insuffisante\n");
        free(data);
        return NULL;
    }
    if (ct3_pack_import_count > RT_PE_MAX_IMPORTS) {
        fprintf(stderr, "rt: pack : trop d'imports (%u)\n", (unsigned)ct3_pack_import_count);
        free(data);
        free_image(img);
        return NULL;
    }

    for (unsigned i = 0; i < ct3_pack_import_count; i++) {
        const ct3_pack_import *imp = &ct3_pack_imports[i];
        uint32_t iat = CT3_IMAGE_BASE + imp->iat_rva;
        int found = 0;
        char name[512];

        for (unsigned j = 0; j < ct3_pack_section_count; j++) {
            if (ct3_pack_sections[j].present && section_contains(&ct3_pack_sections[j], iat, 4)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "rt: pack : IAT 0x%08x hors des sections présentes\n", iat);
            free(data);
            free_image(img);
            return NULL;
        }

        if (imp->name) {
            snprintf(name, sizeof name, "%s::%s", imp->dll, imp->name);
        } else {
            snprintf(name, sizeof name, "%s::#%u", imp->dll, imp->ordinal);
        }
        img->import_names[i] = strdup(name);
        if (!img->import_names[i]) {
            fprintf(stderr, "rt: pack : mémoire insuffisante\n");
            free(data);
            free_image(img);
            return NULL;
        }
        uint32_t stub = RT_STUB_BASE + 16 * i;
        memcpy(rt_mem_base + iat, &stub, 4);
    }
    img->import_count = ct3_pack_import_count;
    img->entry = CT3_IMAGE_ENTRY;
    img->size = CT3_IMAGE_SIZE;

    for (unsigned i = 0; i < ct3_pack_section_count; i++) {
        const ct3_pack_section *sec = &ct3_pack_sections[i];
        uint32_t sec_size = sec->vsize ? sec->vsize : sec->raw_size;
        int prot;
        if (sec_size == 0) continue;
        /* Le pack ne contient que des données : aucune section n'est exécutée.
         * .text est mappée en lecture seule, même si le PE la marque exécutable. */
        prot = (int)(sec->prot & 3);
        if (prot == 0) prot = 1;
        if (rt_mem_protect(sec->va, sec_size, prot) != 0) {
            free(data);
            free_image(img);
            return NULL;
        }
    }

    rt_set_image_has_text(CT3_IMAGE_HAS_TEXT);
    printf("[pack] chargé : %s (image 0x%08x, %u imports)\n", path, (unsigned)CT3_IMAGE_SIZE,
           (unsigned)img->import_count);
    free(data);
    return img;
}
