/* Polices hôte pour le texte GDI : fichiers trouvés par fontconfig (fc-match), rastérisés par stb_truetype.
 * Voir runtime/src/rt_font.c. */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ouvre (ou réutilise) la police « face » ; -1 si aucune police n'est disponible. */
int rt_font_open(const char *face, int bold, int italic);
/* Ascendant et descendant, en pixels, pour une taille em de « px » pixels. */
void rt_font_metrics(int font, float px, int *ascent, int *descent);
/* Avance horizontale du caractère « cp », en pixels. */
float rt_font_advance(int font, float px, unsigned cp);
/* Bitmap de couverture (8 bits) d'un glyphe ; xoff/yoff : décalage du coin haut-gauche par rapport au point
 * de base. Libérer avec rt_font_glyph_free. NULL pour un caractère sans forme (espace). */
unsigned char *rt_font_glyph(int font, float px, unsigned cp, int *w, int *h, int *xoff, int *yoff);
void rt_font_glyph_free(unsigned char *bitmap);

#ifdef __cplusplus
}
#endif
