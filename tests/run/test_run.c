/* Exécution du jeu depuis le pack de données CT3 (code entièrement traduit).
 * Le pack est cherché automatiquement ; le dossier du jeu (assets) peut être
 * passé en argument ou par CT3_DATA_DIR. Chaque import appelé est tracé sur
 * stderr. Un import non implémenté arrête l'exécution. */

#include "harness.h"
#include "rt_proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    const char *game_dir;

    if (argc > 2) {
        fprintf(stderr, "usage : %s [dossier du jeu]\n", argv[0]);
        return 1;
    }
    game_dir = argc == 2 ? argv[1] : getenv("CT3_DATA_DIR");
    rt_set_game_dir(game_dir && *game_dir ? game_dir : ".");

    if (getenv("CT3_PRESENT_LOG")) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        fprintf(stderr, "[start] ms=%ld\n", (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
    }
    rt_patch_load();
    rt_splash_start();
    rt_pe_image *img = harness_load(argc, argv);
    if (!img) {
        return 1;
    }
    reset_cpu();
    if (rt_process_init(&cpu) != 0) {
        return 1;
    }
    printf("point d'entrée 0x%08x, TEB 0x%08x\n", img->entry, RT_TEB_BASE);
    call_guest(img->entry);
    printf("retour du point d'entrée\n");
    return 0;
}
