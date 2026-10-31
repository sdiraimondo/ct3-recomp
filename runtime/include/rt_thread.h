#pragma once

/* Threads, synchronisation et handles (option B : threads réels de l'hôte).
 *
 * Verrou global rt_big : tenu pendant l'exécution de chaque shim. Les attentes (événements,
 * sections critiques, Sleep, WaitFor*) le relâchent pendant leur blocage, via la condition rt_state.
 * Le code invité s'exécute hors du verrou. */

#include "rt.h"

#include <pthread.h>

#define RT_THREAD_STACK_REGION 0x30000000u
#define RT_THREAD_SLOT        0x00102000u /* TEB (4 Kio) + pile (1 Mio) + garde */
#define RT_THREAD_STACK_SIZE  0x00100000u
#define RT_THREAD_MAX         64
#define RT_TLS_SLOTS          64

typedef struct rt_thread {
    uint32_t id;
    uint32_t handle;
    uint32_t teb;
    uint32_t stack_low;
    uint32_t stack_top;
    cpu_t *cpu;
    pthread_t host;
    int started;
    int finished;
    int suspended;
    int is_timer;
    int timer_stop;
    uint32_t exit_code;
    uint32_t start;
    uint32_t param;
    uint32_t timer_period_ms;
    uint32_t timer_callback;
    uint32_t timer_user;
    uint32_t timer_event;
    uint32_t timer_id;
    uint32_t timer_mode;
    uint32_t tls[RT_TLS_SLOTS];
} rt_thread;

rt_thread *rt_current(void);

void rt_lock(void);
void rt_unlock(void);
void rt_wait_state(void);                 /* attend un changement d'état (verrou tenu) */
int rt_wait_state_timed(uint32_t ms);     /* idem, avec délai ; 0 si délai écoulé */
void rt_notify_state(void);
void rt_sleep_ms(uint32_t ms);

rt_thread *rt_thread_main_init(cpu_t *c);
uint32_t rt_thread_create(uint32_t start, uint32_t param, int suspended, uint32_t *id_out);
int rt_thread_resume(uint32_t handle);
uint32_t rt_thread_exit_code(uint32_t handle, int *finished);
void rt_thread_terminate(uint32_t handle, uint32_t code);
uint32_t rt_timer_create(uint32_t period_ms, uint32_t callback, uint32_t user, uint32_t event, uint32_t mode);
void rt_timer_kill(uint32_t id);

uint32_t rt_event_create(int manual, int initial);
int rt_event_set(uint32_t h, int signaled);
uint32_t rt_mutex_create(int initial_owner);
int rt_mutex_release(uint32_t h);
int rt_handle_close(uint32_t h);
uint32_t rt_file_handle_new(int fd);
int rt_file_fd(uint32_t h);
uint32_t rt_wait_handles(const uint32_t *hs, uint32_t n, int wait_all, uint32_t ms);

void rt_cs_init(uint32_t addr);
void rt_cs_enter(uint32_t addr);
void rt_cs_leave(uint32_t addr);
void rt_cs_delete(uint32_t addr);

void rt_install_crash_handler(void);
