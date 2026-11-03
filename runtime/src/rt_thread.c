#include "rt_thread.h"
#include "rt_proc.h"
#include <fcntl.h>

#include <errno.h>
#include <stdint.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int rt_mem_protect(uint32_t addr, uint32_t size, int prot);

static pthread_mutex_t big_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t state_cond = PTHREAD_COND_INITIALIZER;

static __thread rt_thread *current_thread;

static rt_thread main_thread = {.id = 1};

rt_thread *rt_current(void) { return current_thread ? current_thread : &main_thread; }
static unsigned thread_count;
static uint32_t next_tid = 4;

/* ---- verrou et état ---- */

void rt_lock(void) { pthread_mutex_lock(&big_lock); }
void rt_unlock(void) { pthread_mutex_unlock(&big_lock); }

/* Verrou réentrant par thread : un shim peut appeler du code invité qui appelle d'autres shims. */
static __thread int lock_depth;

void rt_lock_enter(void)
{
    if (lock_depth++ == 0) rt_lock();
}

void rt_lock_leave(void)
{
    if (--lock_depth == 0) rt_unlock();
}
void rt_wait_state(void) { pthread_cond_wait(&state_cond, &big_lock); }
void rt_notify_state(void) { pthread_cond_broadcast(&state_cond); }

int rt_wait_state_timed(uint32_t ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(&state_cond, &big_lock, &ts) != ETIMEDOUT;
}

/* ---- table des handles ---- */

typedef enum { OBJ_NONE, OBJ_THREAD, OBJ_EVENT, OBJ_MUTEX, OBJ_FILE } obj_kind;
typedef struct { obj_kind kind; void *p; } obj;
typedef struct { int manual; int signaled; } event_obj;
typedef struct { uint32_t owner; uint32_t count; } mutex_obj;
typedef struct { int fd; } file_obj;

#define OBJ_MAX 4096
static obj objs[OBJ_MAX];
static unsigned obj_used = 1;

static uint32_t new_obj(obj_kind k, void *p)
{
    if (obj_used >= OBJ_MAX) rt_fatal("trop d'objets de synchronisation");
    objs[obj_used].kind = k;
    objs[obj_used].p = p;
    return 0x10000u + 4u * obj_used++;
}

static obj *find_obj(uint32_t h)
{
    if (h < 0x10000u || (h - 0x10000u) % 4 != 0) return NULL;
    uint32_t i = (h - 0x10000u) / 4;
    if (i == 0 || i >= obj_used || objs[i].kind == OBJ_NONE) return NULL;
    return &objs[i];
}

/* ---- threads ---- */

static void init_teb(uint32_t teb, uint32_t stack_low, uint32_t stack_top)
{
    uint32_t v;
    memset(rt_mem_base + teb, 0, 0x1000);
    v = 0xFFFFFFFFu; memcpy(rt_mem_base + teb + 0x00, &v, 4);
    v = stack_top;   memcpy(rt_mem_base + teb + 0x04, &v, 4);
    v = stack_low;   memcpy(rt_mem_base + teb + 0x08, &v, 4);
    v = teb;         memcpy(rt_mem_base + teb + 0x18, &v, 4);
    v = RT_PEB_BASE; memcpy(rt_mem_base + teb + 0x30, &v, 4);
}

static void push_guest(cpu_t *c, uint32_t v)
{
    uint32_t esp = (uint32_t)cpu_rd(c, 16, 4) - 4;
    guest_st(c, esp, 4, v);
    cpu_wr(c, 16, 4, esp);
}

static void run_timer(rt_thread *t)
{
    cpu_t *c = t->cpu;
    while (1) {
        usleep(t->timer_period_ms * 1000u);
        rt_lock();
        int stop = t->timer_stop;
        rt_unlock();
        if (stop) break;
        if (t->timer_mode & 0x10u) { /* TIME_CALLBACK_EVENT_SET : lpTimeProc est un handle d'événement */
            rt_lock();
            rt_event_set(t->timer_callback, 1);
            rt_unlock();
            continue;
        }
        /* TIMECALLBACK(uTimerID, uMsg, dwUser, dw1, dw2) : stdcall, 5 arguments. */
        cpu_wr(c, 16, 4, t->stack_top);
        push_guest(c, 0);
        push_guest(c, 0);
        push_guest(c, t->timer_user);
        push_guest(c, 0);
        push_guest(c, t->timer_id);
        push_guest(c, RT_HOST_RET);
        dispatch(c, t->timer_callback);
    }
}

static void *thread_entry(void *arg)
{
    rt_thread *t = arg;
    current_thread = t;
    if (t->is_timer) {
        run_timer(t);
    } else {
        cpu_wr(t->cpu, 16, 4, t->stack_top);
        push_guest(t->cpu, t->param);
        push_guest(t->cpu, RT_HOST_RET);
        dispatch(t->cpu, t->start);
        t->exit_code = (uint32_t)cpu_rd(t->cpu, 0, 4);
    }
    rt_lock();
    t->finished = 1;
    rt_notify_state();
    rt_unlock();
    return NULL;
}

static rt_thread *alloc_thread(void)
{
    if (thread_count >= RT_THREAD_MAX) rt_fatal("trop de threads invités");
    unsigned slot = thread_count++;
    rt_thread *t = calloc(1, sizeof *t);
    t->id = next_tid;
    next_tid += 4;
    t->teb = RT_THREAD_STACK_REGION + slot * RT_THREAD_SLOT;
    t->stack_low = t->teb + 0x1000;
    t->stack_top = t->stack_low + RT_THREAD_STACK_SIZE - 16;
    if (rt_mem_protect(t->teb, RT_THREAD_SLOT, 3) != 0) rt_fatal("pile de thread non allouée");
    init_teb(t->teb, t->stack_low, t->stack_top);
    t->cpu = calloc(1, sizeof(cpu_t));
    cpu_wr(t->cpu, RT_FS_OFFSET_REG, 4, t->teb);
    cpu_wr(t->cpu, 16, 4, t->stack_top);
    return t;
}

static void start_host_thread(rt_thread *t)
{
    t->started = 1;
    if (pthread_create(&t->host, NULL, thread_entry, t) != 0) rt_fatal("pthread_create");
    pthread_detach(t->host);
}

rt_thread *rt_thread_main_init(cpu_t *c)
{
    rt_thread *t = &main_thread;
    t->id = 1;
    t->teb = RT_TEB_BASE;
    t->stack_low = RT_STACK_LIMIT;
    t->stack_top = RT_STACK_TOP;
    t->cpu = c;
    t->started = 1;
    t->handle = new_obj(OBJ_THREAD, t);
    current_thread = t;
    return t;
}

uint32_t rt_thread_create(uint32_t start, uint32_t param, int suspended, uint32_t *id_out)
{
    rt_thread *t = alloc_thread();
    t->start = start;
    t->param = param;
    t->handle = new_obj(OBJ_THREAD, t);
    if (suspended) {
        t->suspended = 1;
    } else {
        start_host_thread(t);
    }
    if (id_out) *id_out = t->id;
    return t->handle;
}

int rt_thread_resume(uint32_t h)
{
    obj *o = find_obj(h);
    if (!o || o->kind != OBJ_THREAD) return 0;
    rt_thread *t = o->p;
    if (t->suspended) {
        t->suspended = 0;
        start_host_thread(t);
    }
    return 1;
}

uint32_t rt_thread_exit_code(uint32_t h, int *finished)
{
    obj *o = find_obj(h);
    rt_thread *t = (o && o->kind == OBJ_THREAD) ? o->p : rt_current();
    if (finished) *finished = t->finished;
    return t->finished ? t->exit_code : 259; /* STILL_ACTIVE */
}

void rt_thread_terminate(uint32_t h, uint32_t code)
{
    obj *o = find_obj(h);
    if (!o || o->kind != OBJ_THREAD) return;
    rt_thread *t = o->p;
    if (t->started && !t->finished) {
        pthread_cancel(t->host);
    }
    t->exit_code = code;
    t->finished = 1;
    rt_notify_state();
}

uint32_t rt_timer_create(uint32_t period_ms, uint32_t callback, uint32_t user, uint32_t event, uint32_t mode)
{
    rt_thread *t = alloc_thread();
    t->is_timer = 1;
    t->timer_period_ms = period_ms ? period_ms : 1;
    t->timer_callback = callback;
    t->timer_user = user;
    t->timer_event = event;
    t->timer_mode = mode;
    t->timer_id = t->id;
    t->handle = new_obj(OBJ_THREAD, t);
    start_host_thread(t);
    return t->timer_id;
}

void rt_timer_kill(uint32_t id)
{
    for (unsigned i = 1; i < obj_used; i++) {
        if (objs[i].kind == OBJ_THREAD && ((rt_thread *)objs[i].p)->is_timer &&
            ((rt_thread *)objs[i].p)->timer_id == id) {
            ((rt_thread *)objs[i].p)->timer_stop = 1;
        }
    }
}

/* ---- événements et mutex ---- */

uint32_t rt_event_create(int manual, int initial)
{
    event_obj *e = calloc(1, sizeof *e);
    e->manual = manual;
    e->signaled = initial;
    return new_obj(OBJ_EVENT, e);
}

int rt_event_set(uint32_t h, int signaled)
{
    obj *o = find_obj(h);
    if (!o || o->kind != OBJ_EVENT) return 0;
    ((event_obj *)o->p)->signaled = signaled;
    rt_notify_state();
    return 1;
}

uint32_t rt_mutex_create(int initial_owner)
{
    mutex_obj *m = calloc(1, sizeof *m);
    if (initial_owner) {
        m->owner = rt_current()->id;
        m->count = 1;
    }
    return new_obj(OBJ_MUTEX, m);
}

int rt_mutex_release(uint32_t h)
{
    obj *o = find_obj(h);
    if (!o || o->kind != OBJ_MUTEX) return 0;
    mutex_obj *m = o->p;
    if (m->owner != rt_current()->id) return 0;
    if (--m->count == 0) m->owner = 0;
    rt_notify_state();
    return 1;
}

uint32_t rt_file_handle_new(int fd)
{
    file_obj *f = calloc(1, sizeof *f);
    f->fd = fd;
    return new_obj(OBJ_FILE, f);
}

int rt_file_fd(uint32_t h)
{
    obj *o = find_obj(h);
    return (o && o->kind == OBJ_FILE) ? ((file_obj *)o->p)->fd : -1;
}

int rt_handle_close(uint32_t h)
{
    obj *o = find_obj(h);
    if (!o) return 0;
    if (o->kind == OBJ_FILE) close(((file_obj *)o->p)->fd);
    if (o->kind == OBJ_EVENT || o->kind == OBJ_MUTEX || o->kind == OBJ_FILE) free(o->p);
    o->kind = OBJ_NONE;
    o->p = NULL;
    return 1;
}

/* ---- attentes ---- */

static int signaled_for_me(obj *o)
{
    rt_thread *me = rt_current();
    switch (o->kind) {
    case OBJ_THREAD: return ((rt_thread *)o->p)->finished;
    case OBJ_EVENT: return ((event_obj *)o->p)->signaled;
    case OBJ_MUTEX: {
        mutex_obj *m = o->p;
        return m->owner == 0 || m->owner == me->id;
    }
    default: return 0;
    }
}

static void acquire_for_me(obj *o)
{
    rt_thread *me = rt_current();
    if (o->kind == OBJ_EVENT && !((event_obj *)o->p)->manual) {
        ((event_obj *)o->p)->signaled = 0;
    } else if (o->kind == OBJ_MUTEX) {
        mutex_obj *m = o->p;
        m->owner = me->id;
        m->count++;
    }
}

/* Retourne WAIT_OBJECT_0 + i, WAIT_TIMEOUT (0x102) ou WAIT_FAILED (0xFFFFFFFF). */
uint32_t rt_wait_handles(const uint32_t *hs, uint32_t n, int wait_all, uint32_t ms)
{
    obj *os[64];
    if (n == 0 || n > 64) return 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        os[i] = find_obj(hs[i]);
        if (!os[i]) return 0xFFFFFFFFu;
    }
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (1) {
        if (wait_all) {
            int all = 1;
            for (uint32_t i = 0; i < n; i++) all &= signaled_for_me(os[i]);
            if (all) {
                for (uint32_t i = 0; i < n; i++) acquire_for_me(os[i]);
                return 0;
            }
        } else {
            for (uint32_t i = 0; i < n; i++) {
                if (signaled_for_me(os[i])) {
                    acquire_for_me(os[i]);
                    return i;
                }
            }
        }
        if (ms == 0xFFFFFFFFu) {
            rt_wait_state();
            continue;
        }
        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        uint64_t spent = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000u + (uint64_t)(t1.tv_nsec - t0.tv_nsec) / 1000000u;
        if (spent >= ms) return 0x102;
        rt_wait_state_timed((uint32_t)(ms - spent));
    }
}

/* Sleep : libère le verrou pendant l'attente. */
void rt_sleep_ms(uint32_t ms)
{
    rt_unlock();
    if (ms == 0) {
        sched_yield();
    } else {
        struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
        nanosleep(&ts, NULL);
    }
    rt_lock();
}

/* ---- sections critiques : état hôte indexé par l'adresse invitée ---- */

typedef struct cs_entry { uint32_t addr; uint32_t owner; uint32_t count; struct cs_entry *next; } cs_entry;
static cs_entry *cs_list;

static cs_entry *cs_find(uint32_t addr)
{
    for (cs_entry *e = cs_list; e; e = e->next) if (e->addr == addr) return e;
    return NULL;
}

void rt_cs_init(uint32_t addr)
{
    cs_entry *e = cs_find(addr);
    if (!e) {
        e = calloc(1, sizeof *e);
        e->addr = addr;
        e->next = cs_list;
        cs_list = e;
    }
    e->owner = 0;
    e->count = 0;
}

void rt_cs_enter(uint32_t addr)
{
    cs_entry *e = cs_find(addr);
    if (!e) {
        rt_cs_init(addr);
        e = cs_find(addr);
    }
    uint32_t me = rt_current()->id;
    while (e->owner != 0 && e->owner != me) rt_wait_state();
    e->owner = me;
    e->count++;
}

void rt_cs_leave(uint32_t addr)
{
    cs_entry *e = cs_find(addr);
    if (!e || e->count == 0) return;
    if (--e->count == 0) e->owner = 0;
    rt_notify_state();
}

void rt_cs_delete(uint32_t addr)
{
    for (cs_entry **pp = &cs_list; *pp; pp = &(*pp)->next) {
        if ((*pp)->addr == addr) {
            cs_entry *dead = *pp;
            *pp = dead->next;
            free(dead);
            return;
        }
    }
}

/* Diagnostic : faute mémoire pendant l'exécution invitée. Affiche l'adresse invitée et les registres
 * du thread courant, puis sort sans core dump (code 4). */
#include <signal.h>
#include <unistd.h>

static void crash_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)sig;
    (void)ctx;
    uintptr_t fault = (uintptr_t)info->si_addr;
    uintptr_t base = (uintptr_t)rt_mem_base;
    fprintf(stderr, "rt: dernière instruction invitée 0x%08x\n", rt_cur_insn);
    fprintf(stderr, "rt: faute mémoire (signal %d) à l'adresse hôte %p, invitée 0x%llx\n", sig,
            info->si_addr, fault >= base && fault < base + RT_GUEST_SIZE ? (unsigned long long)(fault - base) : 0ULL);
    rt_probe_dump();
    rt_thread *t = rt_current();
    if (t && t->cpu) {
        fprintf(stderr, "rt: EAX=%08llx ECX=%08llx EDX=%08llx EBX=%08llx ESP=%08llx EBP=%08llx ESI=%08llx EDI=%08llx\n",
                (unsigned long long)cpu_rd(t->cpu, 0, 4), (unsigned long long)cpu_rd(t->cpu, 4, 4),
                (unsigned long long)cpu_rd(t->cpu, 8, 4), (unsigned long long)cpu_rd(t->cpu, 12, 4),
                (unsigned long long)cpu_rd(t->cpu, 16, 4), (unsigned long long)cpu_rd(t->cpu, 20, 4),
                (unsigned long long)cpu_rd(t->cpu, 24, 4), (unsigned long long)cpu_rd(t->cpu, 28, 4));
        /* Pile d'appels invitée (heuristique) : mots de la pile qui pointent dans le code du jeu. */
        uint32_t esp = (uint32_t)cpu_rd(t->cpu, 16, 4);
        int shown = 0;
        for (uint32_t off = 0; off < 0x800 && shown < 12; off += 4) {
            uint32_t v;
            memcpy(&v, rt_mem_base + esp + off, 4);
            if (v >= 0x401000u && v < 0x53b000u) {
                fprintf(stderr, "rt:   pile +0x%x : 0x%08x\n", off, v);
                shown++;
            }
        }
    }
    _exit(4);
}

void rt_install_crash_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
