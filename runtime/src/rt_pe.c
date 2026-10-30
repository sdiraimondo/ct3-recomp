#include "rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rt_pe.h"

int rt_mem_init(void);
int rt_mem_protect(uint32_t addr, uint32_t size, int prot);
void rt_mem_fill(uint32_t addr, uint32_t size, uint8_t value);

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }

static const uint8_t *rva_ptr(const uint8_t *file, size_t fsize, const rt_pe_section *secs, unsigned nsec, uint32_t rva)
{
    for (unsigned i = 0; i < nsec; i++) {
        uint32_t va = secs[i].va;
        uint32_t vsz = secs[i].vsize > secs[i].rawsize ? secs[i].vsize : secs[i].rawsize;
        if (rva >= va && rva < va + vsz) {
            uint32_t off = secs[i].rawptr + (rva - va);
            if (off < fsize) {
                return file + off;
            }
            return NULL;
        }
    }
    return NULL;
}

static int prot_of(uint32_t chars)
{
    int prot = 1; /* lecture */
    if (chars & 0x20000000u) prot |= 4; /* exécutable */
    if (chars & 0x80000000u) prot |= 2; /* écriture */
    return prot;
}

rt_pe_image *rt_pe_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size_t fsize = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *file = malloc(fsize);
    if (!file || fread(file, 1, fsize, f) != fsize) {
        fclose(f);
        free(file);
        fprintf(stderr, "rt_pe_load: lecture impossible\n");
        return NULL;
    }
    fclose(f);

    rt_pe_image *img = calloc(1, sizeof(*img));
    if (rt_mem_init() != 0) {
        return NULL;
    }
    if (fsize < 0x40 || memcmp(file, "MZ", 2) != 0) {
        fprintf(stderr, "rt_pe_load: en-tête MZ absent\n");
        return NULL;
    }
    uint32_t pe = rd32(file + 0x3C);
    if (pe + 24 > fsize || memcmp(file + pe, "PE\0\0", 4) != 0) {
        fprintf(stderr, "rt_pe_load: signature PE absente\n");
        return NULL;
    }
    uint16_t machine = rd16(file + pe + 4);
    uint16_t nsec = rd16(file + pe + 6);
    uint16_t optsz = rd16(file + pe + 20);
    const uint8_t *opt = file + pe + 24;
    if (machine != 0x14c || rd16(opt) != 0x10b) {
        fprintf(stderr, "rt_pe_load: attendu PE32 i386\n");
        return NULL;
    }
    uint32_t image_base = rd32(opt + 28);
    uint32_t size_of_image = rd32(opt + 56);
    uint32_t size_of_headers = rd32(opt + 60);
    uint32_t import_rva = rd32(opt + 96 + 8);
    if (image_base != RT_IMAGE_BASE) {
        fprintf(stderr, "rt_pe_load: base d'image inattendue 0x%08x\n", image_base);
        return NULL;
    }
    if (nsec > RT_PE_MAX_SECTIONS) {
        fprintf(stderr, "rt_pe_load: trop de sections\n");
        return NULL;
    }

    const uint8_t *sh = opt + optsz;
    rt_pe_section secs[RT_PE_MAX_SECTIONS];
    for (unsigned i = 0; i < nsec; i++) {
        const uint8_t *s = sh + 40 * i;
        secs[i].va = rd32(s + 12);
        secs[i].vsize = rd32(s + 8);
        secs[i].rawsize = rd32(s + 16);
        secs[i].rawptr = rd32(s + 20);
        secs[i].chars = rd32(s + 36);
    }

    /* Mapping : tout en lecture-écriture pendant le chargement, puis protections finales. */
    if (rt_mem_protect(RT_IMAGE_BASE, size_of_image, 3) != 0) {
        return NULL;
    }
    rt_mem_fill(RT_IMAGE_BASE, size_of_image, 0);
    memcpy(rt_mem_base + RT_IMAGE_BASE, file, size_of_headers < fsize ? size_of_headers : fsize);
    for (unsigned i = 0; i < nsec; i++) {
        uint32_t n = secs[i].rawsize < secs[i].vsize ? secs[i].rawsize : secs[i].vsize;
        if (secs[i].rawptr + n > fsize) {
            fprintf(stderr, "rt_pe_load: section tronquée\n");
            return NULL;
        }
        memcpy(rt_mem_base + RT_IMAGE_BASE + secs[i].va, file + secs[i].rawptr, n);
    }

    /* Table d'importation : chaque entrée IAT reçoit une adresse de talon (stub). */
    if (import_rva) {
        uint32_t idx = 0;
        for (const uint8_t *d = rva_ptr(file, fsize, secs, nsec, import_rva); d && rd32(d + 12) != 0; d += 20) {
            const char *dll = (const char *)rva_ptr(file, fsize, secs, nsec, rd32(d + 12));
            uint32_t ilt = rd32(d) ? rd32(d) : rd32(d + 16);
            uint32_t iat = rd32(d + 16);
            const uint8_t *thunk = rva_ptr(file, fsize, secs, nsec, ilt);
            if (!dll || !thunk) {
                continue;
            }
            for (uint32_t k = 0; rd32(thunk + 4 * k) != 0; k++) {
                uint32_t entry = rd32(thunk + 4 * k);
                char name[256];
                if (entry & 0x80000000u) {
                    snprintf(name, sizeof name, "%s::#%u", dll, entry & 0xFFFFu);
                } else {
                    const char *fn = (const char *)rva_ptr(file, fsize, secs, nsec, entry) + 2;
                    snprintf(name, sizeof name, "%s::%s", dll, fn);
                }
                if (idx >= RT_PE_MAX_IMPORTS) {
                    fprintf(stderr, "rt_pe_load: trop d'imports\n");
                    return NULL;
                }
                img->import_names[idx] = strdup(name);
                uint32_t stub = RT_STUB_BASE + 16 * idx;
                memcpy(rt_mem_base + RT_IMAGE_BASE + iat + 4 * k, &stub, 4);
                idx++;
            }
        }
        img->import_count = idx;
    }

    for (unsigned i = 0; i < nsec; i++) {
        if (rt_mem_protect(RT_IMAGE_BASE + secs[i].va, secs[i].vsize, prot_of(secs[i].chars)) != 0) {
            return NULL;
        }
    }
    img->entry = RT_IMAGE_BASE + rd32(opt + 16);
    img->size = size_of_image;
    free(file);
    return img;
}
