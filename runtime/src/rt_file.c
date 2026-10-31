#include "rt.h"
#include "rt_file.h"
#include "rt_thread.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERROR_FILE_NOT_FOUND 2
#define ERROR_ACCESS_DENIED  5
#define ERROR_INVALID_HANDLE 6

#define GENERIC_READ  0x80000000u
#define GENERIC_WRITE 0x40000000u

uint32_t rt_error;

static char base_dir[1024] = ".";

const char *rt_file_base(void)
{
    return base_dir;
}

void rt_file_set_base(const char *exe_path)
{
    snprintf(base_dir, sizeof base_dir, "%s", exe_path);
    char *slash = strrchr(base_dir, '/');
    if (slash) {
        *slash = 0;
        if (base_dir[0] == 0) strcpy(base_dir, "/");
    } else {
        strcpy(base_dir, ".");
    }
}

/* Traduit un chemin Win32 : « \ » devient « / », la lettre de lecteur est retirée, le chemin est relatif
 * au dossier de l'exécutable. Chaque composant existant est retrouvé sans tenir compte de la casse. Un
 * composant final absent est gardé tel quel (création de fichier). */
void rt_file_resolve(const char *win, char *out, size_t n)
{
    char rel[1024];
    size_t k = 0;
    const char *p = win;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') p += 2;
    for (; *p && k < sizeof rel - 1; p++) rel[k++] = (*p == '\\') ? '/' : *p;
    rel[k] = 0;

    char cur[1024];
    snprintf(cur, sizeof cur, "%s", base_dir);

    const char *seg = rel;
    while (*seg) {
        while (*seg == '/') seg++;
        if (!*seg) break;
        const char *end = seg;
        while (*end && *end != '/') end++;
        size_t len = (size_t)(end - seg);
        char tok[512];
        if (len >= sizeof tok) len = sizeof tok - 1;
        memcpy(tok, seg, len);
        tok[len] = 0;
        seg = end;

        if (strcmp(tok, ".") == 0) continue;

        char next[1024];
        snprintf(next, sizeof next, "%s/%s", cur, tok);
        if (access(next, F_OK) != 0) {
            DIR *d = opendir(cur);
            if (d) {
                struct dirent *e;
                while ((e = readdir(d)) != NULL) {
                    if (strcasecmp(e->d_name, tok) == 0) {
                        snprintf(next, sizeof next, "%s/%s", cur, e->d_name);
                        break;
                    }
                }
                closedir(d);
            }
        }
        snprintf(cur, sizeof cur, "%s", next);
    }
    snprintf(out, n, "%s", cur);
}

uint32_t rt_file_open(const char *win_path, uint32_t desired, uint32_t disposition)
{
    char path[2048];
    rt_file_resolve(win_path, path, sizeof path);

    int flags;
    if ((desired & GENERIC_READ) && (desired & GENERIC_WRITE)) {
        flags = O_RDWR;
    } else if (desired & GENERIC_WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDONLY;
    }
    switch (disposition) {
    case 1: flags |= O_CREAT | O_EXCL; break;  /* CREATE_NEW */
    case 2: flags |= O_CREAT | O_TRUNC; break; /* CREATE_ALWAYS */
    case 4: flags |= O_CREAT; break;           /* OPEN_ALWAYS */
    case 5: flags |= O_TRUNC; break;           /* TRUNCATE_EXISTING */
    default: break;                            /* OPEN_EXISTING */
    }

    int fd = open(path, flags, 0644);
    if (fd < 0) {
        rt_error = (errno == ENOENT || errno == ENOTDIR) ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED;
        return RT_INVALID_HANDLE;
    }
    rt_error = 0;
    return rt_file_handle_new(fd);
}

int rt_file_read(uint32_t h, uint32_t buf, uint32_t n, uint32_t got_addr)
{
    int fd = rt_file_fd(h);
    if (fd < 0) {
        rt_error = ERROR_INVALID_HANDLE;
        return 0;
    }
    ssize_t r = read(fd, rt_mem_base + buf, n);
    if (r < 0) {
        rt_error = ERROR_ACCESS_DENIED;
        return 0;
    }
    if (got_addr) {
        uint32_t got = (uint32_t)r;
        memcpy(rt_mem_base + got_addr, &got, 4);
    }
    {
        static int log = -1;
        if (log < 0) log = getenv("CT3_FILELOG") != NULL;
        if (log) fprintf(stderr, "[lecture] h=0x%x demande=0x%x lu=%zd\n", h, n, r);
    }
    rt_error = 0;
    return 1;
}

int rt_file_write(uint32_t h, uint32_t buf, uint32_t n, uint32_t got_addr)
{
    int fd = rt_file_fd(h);
    if (fd < 0) {
        rt_error = ERROR_INVALID_HANDLE;
        return 0;
    }
    ssize_t w = write(fd, rt_mem_base + buf, n);
    if (w < 0) {
        rt_error = ERROR_ACCESS_DENIED;
        return 0;
    }
    if (got_addr) {
        uint32_t got = (uint32_t)w;
        memcpy(rt_mem_base + got_addr, &got, 4);
    }
    rt_error = 0;
    return 1;
}

uint32_t rt_file_seek(uint32_t h, uint32_t dist_low, uint32_t dist_high_addr, uint32_t method)
{
    int fd = rt_file_fd(h);
    if (fd < 0) {
        rt_error = ERROR_INVALID_HANDLE;
        return RT_INVALID_HANDLE;
    }
    int64_t dist = (int32_t)dist_low;
    if (dist_high_addr) {
        int32_t high;
        memcpy(&high, rt_mem_base + dist_high_addr, 4);
        dist = (int64_t)(((uint64_t)(uint32_t)high << 32) | dist_low);
    }
    int whence = method == 1 ? SEEK_CUR : method == 2 ? SEEK_END : SEEK_SET;
    off_t pos = lseek(fd, (off_t)dist, whence);
    if (pos < 0) {
        rt_error = ERROR_ACCESS_DENIED;
        return RT_INVALID_HANDLE;
    }
    if (dist_high_addr) {
        uint32_t high = (uint32_t)((uint64_t)pos >> 32);
        memcpy(rt_mem_base + dist_high_addr, &high, 4);
    }
    rt_error = 0;
    return (uint32_t)pos;
}

uint32_t rt_file_size(uint32_t h, uint32_t high_addr)
{
    int fd = rt_file_fd(h);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        rt_error = ERROR_INVALID_HANDLE;
        return RT_INVALID_HANDLE;
    }
    if (high_addr) {
        uint32_t high = (uint32_t)((uint64_t)st.st_size >> 32);
        memcpy(rt_mem_base + high_addr, &high, 4);
    }
    rt_error = 0;
    return (uint32_t)st.st_size;
}
