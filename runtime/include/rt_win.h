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
void rt_win_key_states(uint8_t out[256]);  /* état clavier et manette par code DIK */
/* diagnostic : bouton Start de la manette virtuelle (CT3_PAD_TEST, CT3_PAD_SEQ) */
void rt_win_pad_test(int down, const char *name);

/* Manette DirectInput (rt_extra.cpp) : réglages de [Joystick] et état SDL (axes LX LY RX RY LT RT, boutons 0 à 14). */
typedef struct {
    int enabled;   /* 1 : la manette est exposée au jeu */
    int stick_x;   /* 1 : stick gauche sur l'axe X */
    int stick_y;   /* 1 : stick gauche sur l'axe Y */
    int dpad_pov;  /* 0 : croix en axes (X et Y) ; 1 : croix en POV */
    int triggers_z; /* 0 : gâchettes sur l'axe Y (RT = bas, LT = haut) ; 1 : sur l'axe Z */
    int deadzone;  /* zone morte des sticks et des gâchettes, unités SDL (0 à 32766) */
    unsigned char guid[16];  /* GUID d'instance du périphérique (octets DirectInput) */
} rt_joy_cfg;
extern rt_joy_cfg rt_joy_config;
int rt_win_joy_connected(void);
void rt_win_joy_state(int16_t axes[6], uint8_t buttons[15]);
#ifdef __cplusplus
}
#endif
