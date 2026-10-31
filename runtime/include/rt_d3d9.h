#pragma once

/* Pont Direct3D 9 : objets COM invités (vtables de talons) vers les interfaces hôtes de dxvk-native.
 *
 * Un objet invité fait 8 octets : [vtable][identifiant]. L'identifiant indexe la table des objets
 * hôtes. Une même interface hôte n'a qu'un seul objet invité (identité COM préservée).
 * Les pointeurs vers la mémoire invitée sont directement utilisables côté hôte (rt_mem_base) ; les
 * structures contenant des pointeurs (D3DPRESENT_PARAMETERS, D3DLOCKED_RECT) sont converties. */

#include "rt_shim.h"
#include "rt_proc.h"
#include "rt_win.h"

#include <d3d9.h>

#include <cstring>

extern "C" {

/* Direct3DCreate9 (appelé par le talon d3d9.dll::Direct3DCreate9). */
uint32_t rt_d3d9_create(uint32_t sdk_version);

/* Helpers du code généré (gen_com.py). */
void *rt_com_host(uint32_t guest);
void rt_com_out(uint32_t guest_slot, void *host, int ifidx);
uint32_t rt_com_wrap(void *host, int ifidx);
void rt_com_forget(void *host);
HWND rt_hwnd(uint32_t guest);
float rt_u2f(uint32_t v);

/* Objet invité et ses données hôtes (tampons verrouillés, longueurs). */
typedef struct rt_com_obj {
    void *host;
    int ifidx;
    uint32_t guest;
    uint32_t extra;
    uint32_t dxt;  /* format BC logique si la texture hôte est un repli X8R8G8B8 (0 sinon) */
} rt_com_obj;

rt_com_obj *rt_com_obj_of(uint32_t guest);
int rt_com_iface_index(const char *name);

}  // extern "C"

void rt_pp_in(uint32_t guest, D3DPRESENT_PARAMETERS &out);

inline void *rt_gptr(uint32_t guest)
{
    return guest ? static_cast<void *>(rt_mem_base + guest) : nullptr;
}

template <typename T>
inline T *rt_self(uint32_t guest)
{
    return static_cast<T *>(rt_com_host(guest));
}
