#pragma once

/* Fenêtres Win32 (user32) : chaque HWND invité est une fenêtre SDL2 (surface Vulkan pour dxvk). */

#include "rt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise SDL (vidéo) et le WSI de dxvk avant la première interface D3D9. */
void rt_win_init(void);

/* Dimensionne une fenêtre au back buffer. Plein écran : sans bordure, en (0,0), sans changer le mode. */
void rt_win_fit(uint32_t hwnd, uint32_t width, uint32_t height, int windowed);

/* Fenêtre hôte (SDL_Window *) associée à un HWND invité, ou NULL. */
void *rt_win_host(uint32_t hwnd);

/* HWND invité associé à une fenêtre hôte, ou 0. */
uint32_t rt_win_guest(void *host);

void rt_win_test_key(int down);
void rt_win_key_states(uint8_t out[256]);  /* état clavier par code DIK */  /* diagnostic : touche Entrée poussée dans la file SDL (CT3_KEY_AT) */
#ifdef __cplusplus
}
#endif
