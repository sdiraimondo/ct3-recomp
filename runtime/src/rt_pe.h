#pragma once

#include <stdint.h>

#define RT_PE_MAX_SECTIONS 32
#define RT_PE_MAX_IMPORTS 1024

typedef struct rt_pe_section {
    uint32_t va;
    uint32_t vsize;
    uint32_t rawsize;
    uint32_t rawptr;
    uint32_t chars;
} rt_pe_section;

typedef struct rt_pe_image {
    uint32_t entry;
    uint32_t size;
    uint32_t import_count;
    char *import_names[RT_PE_MAX_IMPORTS];
} rt_pe_image;

rt_pe_image *rt_pe_load(const char *path);
