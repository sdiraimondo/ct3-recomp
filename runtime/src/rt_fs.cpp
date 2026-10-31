// Système de fichiers Win32 complémentaire (KERNEL32) : espace disque, recherche de fichiers,
// chemins complets, suppression et renommage, horodatages. Les chemins passent par rt_file.

#include "rt_file.h"
#include "rt_proc.h"
#include "rt_shim.h"
#include "rt_thread.h"

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {

constexpr uint32_t kErrorFileNotFound = 2;
constexpr uint32_t kErrorNoMoreFiles = 18;
constexpr uint32_t kFileAttributeDirectory = 0x10;
constexpr uint32_t kFileAttributeNormal = 0x80;
constexpr uint32_t kFindBase = 0x00070000u;
constexpr uint64_t kUnixToFileTime = 11644473600ULL;

struct find_state {
    std::string dir;      // dossier hôte (résolu)
    std::string pattern;  // motif Win32 (ex. *.bin)
    DIR *handle;
};

std::map<uint32_t, find_state> finds;
uint32_t next_find = kFindBase;

uint32_t get32(uint32_t addr)
{
    uint32_t v;
    memcpy(&v, rt_mem_base + addr, 4);
    return v;
}

void put32(uint32_t addr, uint32_t v)
{
    memcpy(rt_mem_base + addr, &v, 4);
}

std::string cstr(uint32_t addr, size_t max = 4096)
{
    std::string s;
    if (!addr) return s;
    for (size_t i = 0; i < max; i++) {
        char ch = (char)rt_mem_base[addr + i];
        if (!ch) break;
        s.push_back(ch);
    }
    return s;
}

void put_cstr(uint32_t dst, const std::string &s, uint32_t max)
{
    if (!max) return;
    uint32_t n = (uint32_t)s.size() < max - 1 ? (uint32_t)s.size() : max - 1;
    memcpy(rt_mem_base + dst, s.data(), n);
    rt_mem_base[dst + n] = 0;
}

void put_filetime(uint32_t addr, time_t sec)
{
    uint64_t ft = ((uint64_t)sec + kUnixToFileTime) * 10000000ULL;
    put32(addr, (uint32_t)ft);
    put32(addr + 4, (uint32_t)(ft >> 32));
}

// Remplit WIN32_FIND_DATAA (320 octets) pour une entrée de répertoire.
void fill_find_data(uint32_t out, const std::string &dir, const char *name)
{
    struct stat st;
    std::string full = dir + "/" + name;
    memset(rt_mem_base + out, 0, 320);
    bool ok = stat(full.c_str(), &st) == 0;
    bool isdir = ok && S_ISDIR(st.st_mode);
    uint64_t size = ok ? (uint64_t)st.st_size : 0;
    put32(out + 0, isdir ? kFileAttributeDirectory : kFileAttributeNormal);
    time_t mt = ok ? st.st_mtime : 0;
    put_filetime(out + 4, mt);
    put_filetime(out + 12, mt);
    put_filetime(out + 20, mt);
    put32(out + 28, (uint32_t)(size >> 32));
    put32(out + 32, (uint32_t)size);
    put_cstr(out + 44, name, 260);
}

// Avance jusqu'à la prochaine entrée correspondant au motif (insensible à la casse).
bool next_match(find_state &fs, const char **name_out)
{
    struct dirent *e;
    while ((e = readdir(fs.handle)) != nullptr) {
        if (fnmatch(fs.pattern.c_str(), e->d_name, FNM_CASEFOLD) == 0) {
            *name_out = e->d_name;
            return true;
        }
    }
    return false;
}

uint32_t sh_GetDiskFreeSpaceExA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (lpDirectoryName, lpFreeBytesAvailable, lpTotalNumberOfBytes, lpTotalNumberOfFreeBytes)
    const uint64_t total = 512ULL << 30;
    const uint64_t freebytes = 256ULL << 30;
    if (a[1]) put32(a[1], (uint32_t)freebytes), put32(a[1] + 4, (uint32_t)(freebytes >> 32));
    if (a[2]) put32(a[2], (uint32_t)total), put32(a[2] + 4, (uint32_t)(total >> 32));
    if (a[3]) put32(a[3], (uint32_t)freebytes), put32(a[3] + 4, (uint32_t)(freebytes >> 32));
    return 1;
}

uint32_t sh_FindFirstFileA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    std::string win = cstr(a[0]);
    size_t slash = win.find_last_of("/\\");
    std::string dir_win = slash == std::string::npos ? std::string(".") : win.substr(0, slash);
    std::string pattern = slash == std::string::npos ? win : win.substr(slash + 1);
    char dir[1024];
    rt_file_resolve(dir_win.c_str(), dir, sizeof dir);
    DIR *d = opendir(dir);
    if (!d) {
        rt_error = kErrorFileNotFound;
        return 0xFFFFFFFFu;
    }
    find_state fs{dir, pattern, d};
    const char *name = nullptr;
    if (!next_match(fs, &name)) {
        closedir(d);
        rt_error = kErrorFileNotFound;
        return 0xFFFFFFFFu;
    }
    fill_find_data(a[1], fs.dir, name);
    uint32_t h = next_find;
    next_find += 0x10;
    finds[h] = fs;
    rt_error = 0;
    return h;
}

uint32_t sh_FindNextFileA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    auto it = finds.find(a[0]);
    if (it == finds.end()) return 0;
    const char *name = nullptr;
    if (!next_match(it->second, &name)) {
        rt_error = kErrorNoMoreFiles;
        return 0;
    }
    fill_find_data(a[1], it->second.dir, name);
    return 1;
}

uint32_t sh_FindClose(cpu_t *c, const uint32_t *a)
{
    (void)c;
    auto it = finds.find(a[0]);
    if (it == finds.end()) return 0;
    closedir(it->second.handle);
    finds.erase(it);
    return 1;
}

uint32_t sh_GetFullPathNameA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (lpFileName, nBufferLength, lpBuffer, lpFilePart)
    char resolved[1100];
    rt_file_resolve(cstr(a[0]).c_str(), resolved, sizeof resolved);
    size_t len = strlen(resolved);
    if (len + 1 > a[1]) return (uint32_t)(len + 1);
    memcpy(rt_mem_base + a[2], resolved, len + 1);
    if (a[3]) {
        const char *last = strrchr(resolved, '/');
        put32(a[3], a[2] + (uint32_t)(last ? last - resolved + 1 : 0));
    }
    return (uint32_t)len;
}

uint32_t sh_DeleteFileA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    char path[1100];
    rt_file_resolve(cstr(a[0]).c_str(), path, sizeof path);
    if (unlink(path) != 0) {
        rt_error = kErrorFileNotFound;
        return 0;
    }
    return 1;
}

uint32_t sh_MoveFileA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    char from[1100], to[1100];
    rt_file_resolve(cstr(a[0]).c_str(), from, sizeof from);
    rt_file_resolve(cstr(a[1]).c_str(), to, sizeof to);
    if (rename(from, to) != 0) {
        rt_error = kErrorFileNotFound;
        return 0;
    }
    return 1;
}

uint32_t sh_GetFileTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime) : seule la date de modification est connue.
    int fd = rt_file_fd(a[0]);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) return 0;
    if (a[1]) put_filetime(a[1], st.st_mtime);
    if (a[2]) put_filetime(a[2], st.st_mtime);
    if (a[3]) put_filetime(a[3], st.st_mtime);
    return 1;
}

uint32_t sh_FileTimeToLocalFileTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    memcpy(rt_mem_base + a[1], rt_mem_base + a[0], 8);
    return 1;
}

uint32_t sh_FileTimeToSystemTime(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // FILETIME (100 ns depuis 1601) -> SYSTEMTIME (8 mots).
    uint64_t ft = ((uint64_t)get32(a[0] + 4) << 32) | get32(a[0]);
    time_t sec = (time_t)(ft / 10000000ULL) - (time_t)kUnixToFileTime;
    struct tm t;
    gmtime_r(&sec, &t);
    uint16_t w[8] = {(uint16_t)(t.tm_year + 1900), (uint16_t)(t.tm_mon + 1), (uint16_t)t.tm_wday,
                     (uint16_t)t.tm_mday, (uint16_t)t.tm_hour, (uint16_t)t.tm_min, (uint16_t)t.tm_sec,
                     (uint16_t)((ft / 10000ULL) % 1000ULL)};
    memcpy(rt_mem_base + a[1], w, sizeof w);
    return 1;
}

// Projections de fichiers : CreateFileMappingA renvoie un handle ; MapViewOfFile copie la zone
// demandée dans le tas invité (lecture seule : les écritures ne retournent pas au fichier).
struct mapping_state {
    uint32_t file;
    uint64_t size;
};

std::map<uint32_t, mapping_state> mappings;
uint32_t next_mapping = 0x00080000u;

uint32_t sh_CreateFileMappingA(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hFile, lpAttributes, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName)
    int fd = rt_file_fd(a[0]);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        rt_error = 6;  // ERROR_INVALID_HANDLE
        return 0;
    }
    uint32_t h = next_mapping;
    next_mapping += 0x10;
    mappings[h] = mapping_state{a[0], (uint64_t)st.st_size};
    rt_error = 0;
    return h;
}

uint32_t sh_MapViewOfFile(cpu_t *c, const uint32_t *a)
{
    (void)c;
    // (hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap)
    auto it = mappings.find(a[0]);
    if (it == mappings.end()) return 0;
    uint64_t off = ((uint64_t)a[2] << 32) | a[3];
    uint64_t size = a[4] ? a[4] : it->second.size - off;
    int fd = rt_file_fd(it->second.file);
    if (fd < 0 || size == 0) return 0;
    uint32_t dst = rt_heap_alloc((uint32_t)size, 0);
    size_t got = 0;
    while (got < size) {
        ssize_t r = pread(fd, rt_mem_base + dst + got, size - got, (off_t)(off + got));
        if (r <= 0) break;
        got += (size_t)r;
    }
    return dst;
}

uint32_t sh_UnmapViewOfFile(cpu_t *c, const uint32_t *a)
{
    (void)c;
    rt_heap_free(a[0]);
    return 1;
}

}  // namespace

extern "C" const rt_shim_entry rt_fs_table[] = {
    {"KERNEL32.dll::CreateFileMappingA", 6, sh_CreateFileMappingA},
    {"KERNEL32.dll::MapViewOfFile", 5, sh_MapViewOfFile},
    {"KERNEL32.dll::UnmapViewOfFile", 1, sh_UnmapViewOfFile},
    {"KERNEL32.dll::GetDiskFreeSpaceExA", 4, sh_GetDiskFreeSpaceExA},
    {"KERNEL32.dll::FindFirstFileA", 2, sh_FindFirstFileA},
    {"KERNEL32.dll::FindNextFileA", 2, sh_FindNextFileA},
    {"KERNEL32.dll::FindClose", 1, sh_FindClose},
    {"KERNEL32.dll::GetFullPathNameA", 4, sh_GetFullPathNameA},
    {"KERNEL32.dll::DeleteFileA", 1, sh_DeleteFileA},
    {"KERNEL32.dll::MoveFileA", 2, sh_MoveFileA},
    {"KERNEL32.dll::GetFileTime", 4, sh_GetFileTime},
    {"KERNEL32.dll::FileTimeToLocalFileTime", 2, sh_FileTimeToLocalFileTime},
    {"KERNEL32.dll::FileTimeToSystemTime", 2, sh_FileTimeToSystemTime},
};

extern "C" const unsigned rt_fs_count = sizeof rt_fs_table / sizeof rt_fs_table[0];
