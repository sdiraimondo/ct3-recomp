#pragma once

#include <stdint.h>

/* Fichiers Win32 : chemins relatifs au dossier de l'exécutable, insensibles à la casse. */

#define RT_INVALID_HANDLE 0xFFFFFFFFu

extern uint32_t rt_error; /* dernière erreur (GetLastError) */

void rt_file_set_base(const char *exe_path);
uint32_t rt_file_open(const char *win_path, uint32_t access, uint32_t disposition);
int rt_file_read(uint32_t h, uint32_t buf, uint32_t n, uint32_t got_addr);
int rt_file_write(uint32_t h, uint32_t buf, uint32_t n, uint32_t got_addr);
uint32_t rt_file_seek(uint32_t h, uint32_t dist_low, uint32_t dist_high_addr, uint32_t method);
uint32_t rt_file_size(uint32_t h, uint32_t high_addr);
