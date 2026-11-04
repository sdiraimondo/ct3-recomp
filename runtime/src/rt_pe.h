#pragma once

#include <stdint.h>

#define RT_PE_MAX_IMPORTS 1024

typedef struct rt_pe_image {
    uint32_t entry;
    uint32_t size;
    uint32_t import_count;
    char *import_names[RT_PE_MAX_IMPORTS];
} rt_pe_image;

/* Charge l'image invitée depuis le pack de données CT3 (lot 1).
 *
 * pack_path :
 *   - NULL ou "" : recherche automatique (CT3_PACK, répertoire courant,
 *     à côté du binaire, ../pack, ../../pack, ../share/ct3) ;
 *   - un fichier ct3_data.bin ;
 *   - un dossier contenant ct3_data.bin.
 *
 * argv0 sert à localiser le pack à côté du binaire ; il peut être NULL.
 * Le runtime ne lit jamais CT3.exe. */
rt_pe_image *rt_pe_load(const char *pack_path, const char *argv0);

/* Indique si le pack contient les octets de .text (inclus par défaut, comme
 * données). Utilisé par rt_unresolved pour suivre les thunks non traduits. */
void rt_set_image_has_text(int has_text);
int rt_image_has_text(void);
