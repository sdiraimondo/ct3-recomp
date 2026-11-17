/*
 * Sonde de fonctionnalités devkitA64/libnx pour le port Switch (tâche S1).
 *
 * Ce fichier n'est pas exécuté : il est compilé et lié avec la toolchain
 * Switch pour vérifier que les primitives dont le runtime a besoin existent
 * réellement (déclarations et symboles), et lesquelles manquent.
 *
 * Compilation (dans l'image devkitpro/devkita64) :
 *   ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
 *   aarch64-none-elf-gcc -O2 $ARCH -I$DEVKITPRO/libnx/include -D__SWITCH__ \
 *       -c switch_features.c -o switch_features.o
 *   aarch64-none-elf-gcc -specs=$DEVKITPRO/libnx/switch.specs $ARCH \
 *       switch_features.o -o switch_features.elf -L$DEVKITPRO/libnx/lib -lnx
 */

#include <pthread.h>
#include <switch.h>
#include <sys/time.h>
#include <time.h>

static __thread int tls_value;

static void thread_entry(void *arg)
{
    (void)arg;
    tls_value = 1;
}

static void *pthread_entry(void *arg)
{
    (void)arg;
    tls_value = 2;
    return NULL;
}

int main(void)
{
    Thread thread;
    pthread_t pthread;
    struct timespec ts;
    struct timeval tv;
    VirtmemReservation *reservation;
    void *slice;

    /* Threads libnx (backend prévu pour rt_thread.c). */
    if (R_SUCCEEDED(threadCreate(&thread, thread_entry, NULL, NULL, 0x4000,
                                 0x2C, -2))) {
        threadStart(&thread);
        threadWaitForExit(&thread);
        threadClose(&thread);
    }

    /* Threads POSIX : présents dans libsysbase, à vérifier au link. */
    (void)pthread_create(&pthread, NULL, pthread_entry, NULL);
    (void)pthread_join(pthread, NULL);

    /* Temps. */
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)gettimeofday(&tv, NULL);
    (void)svcGetSystemTick();

    /* Mémoire : pas de mmap, on passe par le gestionnaire virtuel de libnx. */
    virtmemLock();
    slice = virtmemFindAslr(0x1000, 0);
    reservation = slice != NULL ? virtmemAddReservation(slice, 0x1000) : NULL;
    if (reservation != NULL) {
        virtmemRemoveReservation(reservation);
    }
    virtmemUnlock();

    return tls_value;
}
