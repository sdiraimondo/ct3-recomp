#pragma once

#include "rt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*rt_shim_fn)(cpu_t *c, const uint32_t *args);

/* Entrée de talon : nom « DLL::fonction » (ou « d3d9com::Interface::méthode »), arguments stdcall
 * (nombre de mots de 32 bits dépilés, this compris) et fonction hôte. */
typedef struct rt_shim_entry {
    const char *name;
    unsigned nargs;
    rt_shim_fn fn;
} rt_shim_entry;

/* Interface COM exposée au code invité (vtable construite à la demande). */
typedef struct rt_com_iface {
    const char *name;
    const rt_shim_entry *methods;
    unsigned count;
} rt_com_iface;

extern const rt_com_iface rt_com_ifaces[];
extern const unsigned rt_com_iface_count;

/* Tables de talons par DLL (définies dans rt_win.cpp, ...). */
extern const rt_shim_entry rt_user32_table[];
extern const unsigned rt_user32_count;
extern const rt_shim_entry rt_extra_table[];
extern const unsigned rt_extra_count;
extern const rt_shim_entry rt_fs_table[];
extern const unsigned rt_fs_count;

/* Appel d'une fonction invitée depuis l'hôte (rappel stdcall : WndProc, TIMECALLBACK...). */
uint32_t rt_call_guest(cpu_t *c, uint32_t fn, const uint32_t *args, unsigned n);

/* Adresse de talon pour un nom donné (crée le talon dynamique si nécessaire). */
uint32_t rt_dyn_stub_addr(const char *full);

/* Verrou global : réentrant par thread (un appel de shim peut en invoquer d'autres). */
void rt_lock_enter(void);
void rt_lock_leave(void);

#ifdef __cplusplus
}
#endif
