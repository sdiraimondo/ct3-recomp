#pragma once

/* Fenêtres Win32 (user32) : chaque HWND invité est une fenêtre SDL2 (surface Vulkan pour dxvk). */

#include "rt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise SDL (vidéo) et le WSI de dxvk avant la première interface D3D9. */
void rt_win_init(void);

/* Fenêtre hôte (SDL_Window *) associée à un HWND invité, ou NULL. */
void *rt_win_host(uint32_t hwnd);

/* HWND invité associé à une fenêtre hôte, ou 0. */
uint32_t rt_win_guest(void *host);

#ifdef __cplusplus
}
#endif
