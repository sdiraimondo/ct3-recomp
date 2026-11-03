/* Exécution de CT3.exe depuis son point d'entrée (code entièrement traduit).
 * Chaque import appelé est tracé sur stderr. Un import non implémenté arrête l'exécution. */

#include "harness.h"
#include "rt_proc.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    if (getenv("CT3_PRESENT_LOG")) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        fprintf(stderr, "[start] ms=%ld\n", (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000));
    }
    if (argc == 2) rt_splash_start(argv[1]);
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
