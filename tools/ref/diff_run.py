#!/usr/bin/env python3
"""Exécution de référence de CT3.exe dans Unicorn (x86 réel), pour comparer la séquence
d'appels d'imports à celle du runtime traduit (ct3_run).

Les imports sont interceptés par des talons ; leur comportement est simplifié (valeurs
plausibles). Le but est la séquence (nom, arguments) : la première divergence avec ct3_run
indique l'endroit où la traduction se comporte autrement que le code d'origine.

Sortie : une ligne par import, au même format que ct3_run : "[import] (retour 0x…) nom(args)".
"""

import argparse
import os
import struct
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_PROT_ALL, UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_ECX

IMAGE_BASE = 0x400000
STUB_BASE = 0x7F000000
SENTINEL = 0x7EFFFFF0
STACK_TOP = 0x10300000 + 0xFF00
HEAP_BASE = 0x20000000


def pe_parse(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    entry = IMAGE_BASE + struct.unpack_from("<I", data, opt + 16)[0]
    size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
    hdr = struct.unpack_from("<I", data, opt + 60)[0]
    imp_rva = struct.unpack_from("<I", data, opt + 96 + 8)[0]
    secs = []
    for i in range(nsec):
        s = opt + optsz + 40 * i
        vsz, va, rawsz, rawptr, chars = struct.unpack_from("<IIIII", data, s + 8)[0:2] + struct.unpack_from("<III", data, s + 16)[0:2] + (struct.unpack_from("<I", data, s + 36)[0],)
        secs.append((va, vsz, rawptr, rawsz, chars))
    return data, entry, size_of_image, hdr, secs, imp_rva


def rva2off(secs, rva):
    for va, vsz, rawptr, rawsz, chars in secs:
        if va <= rva < va + max(vsz, rawsz):
            return rva - va + rawptr
    return None


def imports(data, secs, imp_rva):
    """Retourne la liste (adresse IAT, nom « DLL::fonction »)."""
    out = []
    off = rva2off(secs, imp_rva)
    i = 0
    while True:
        orig, _, _, name_rva, first = struct.unpack_from("<IIIII", data, off + 20 * i)
        if name_rva == 0:
            break
        no = rva2off(secs, name_rva)
        dll = data[no:data.index(b"\0", no)].decode()
        tho = rva2off(secs, orig or first)
        k = 0
        while True:
            v = struct.unpack_from("<I", data, tho + 4 * k)[0]
            if v == 0:
                break
            if v & 0x80000000:
                nm = "#%d" % (v & 0xFFFF)
            else:
                ho = rva2off(secs, v)
                nm = data[ho + 2:data.index(b"\0", ho + 2)].decode()
            out.append((IMAGE_BASE + first + 4 * k, "%s::%s" % (dll, nm)))
            k += 1
        i += 1
    return out


# Comportements simplifiés : nom court -> (nombre d'arguments, valeur de retour ou fonction)
NARGS = {
    "GetVersion": 0, "#17": 0, "GetVersionExA": 1, "GetModuleHandleA": 1, "GetCommandLineA": 0,
    "GetStartupInfoA": 1, "HeapCreate": 3, "HeapAlloc": 3, "HeapFree": 3, "HeapSize": 3, "HeapReAlloc": 4,
    "GetProcessHeap": 0, "GetStdHandle": 1, "SetHandleCount": 1, "GetFileType": 1,
    "GetEnvironmentStringsW": 0, "FreeEnvironmentStringsW": 1, "GetACP": 0, "GetOEMCP": 0, "GetCPInfo": 2,
    "IsValidCodePage": 1, "InitializeCriticalSection": 1, "EnterCriticalSection": 1, "LeaveCriticalSection": 1,
    "DeleteCriticalSection": 1, "InitializeCriticalSectionAndSpinCount": 2, "TlsAlloc": 0, "TlsGetValue": 1,
    "TlsSetValue": 2, "TlsFree": 1, "FlsAlloc": 1, "FlsGetValue": 1, "FlsSetValue": 2, "FlsFree": 1,
    "SetUnhandledExceptionFilter": 1, "GetCurrentThreadId": 0, "GetCurrentProcessId": 0, "GetTickCount": 0,
    "GetLastError": 0, "SetLastError": 1, "GetProcAddress": 2, "MultiByteToWideChar": 6,
    "WideCharToMultiByte": 8, "GetStringTypeW": 4, "GetStringTypeA": 5, "LCMapStringW": 6, "LCMapStringA": 6,
    "GetModuleFileNameA": 3, "IsProcessorFeaturePresent": 1, "GetSystemTimeAsFileTime": 1, "Sleep": 1,
    "QueryPerformanceCounter": 1, "QueryPerformanceFrequency": 1, "InterlockedIncrement": 1,
    "InterlockedDecrement": 1, "InterlockedExchange": 2, "InterlockedCompareExchange": 3,
    "GetCurrentThread": 0, "ExitProcess": 1, "VirtualAlloc": 4, "VirtualFree": 3, "CloseHandle": 1,
    "CreateThread": 6, "timeGetTime": 0, "GetLocalTime": 1, "GetSystemTime": 1,
}


def shim_return(short, a, uc):
    """Valeurs de retour identiques à runtime/src/rt_shims.c (sauf les cas traités dans shim())."""
    one = {"InitializeCriticalSectionAndSpinCount", "CloseHandle", "SetEvent", "ResetEvent", "ReleaseMutex",
           "FlsFree", "FreeEnvironmentStringsW", "HeapFree", "TlsFree",
           "SetThreadPriority", "IsValidCodePage", "GetStringTypeW", "GetStringTypeA", "GetCPInfo",
           "IsProcessorFeaturePresent", "QueryPerformanceCounter",
           "QueryPerformanceFrequency", "GetSystemTimeAsFileTime", "GetCurrentThread"}
    if short in one:
        if short == "IsProcessorFeaturePresent":
            return 1 if a[0] in (3, 6, 7, 8, 10) else 0
        return 1
    if short in ("LCMapStringW", "LCMapStringA"):
        return a[3]
    if short in ("MultiByteToWideChar",):
        n = a[3] if a[3] != 0xFFFFFFFF else len(read_bytes_cstr(uc, a[2])) + 1
        return n
    if short in ("WideCharToMultiByte",):
        n = a[3] if a[3] != 0xFFFFFFFF else (len(read_words_cstr(uc, a[2])) + 1)
        return n
    if short == "SetHandleCount":
        return a[0]
    return None


def read_bytes_cstr(uc, addr):
    out = b""
    while True:
        c = bytes(uc.mem_read(addr + len(out), 1))
        if c == b"\0":
            return out
        out += c


def read_words_cstr(uc, addr):
    out = []
    while True:
        w = bytes(uc.mem_read(addr + 2 * len(out), 2))
        if w == b"\0\0":
            return out
        out.append(w)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("--max", type=int, default=400)
    ap.add_argument("--insns", type=int, default=50_000_000)
    args = ap.parse_args()

    data, entry, size_of_image, hdr, secs, imp_rva = pe_parse(args.exe)
    imps = imports(data, secs, imp_rva)
    stub_name = {}
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    # TEB à l'adresse 0 (fs:[x] = [x] dans Unicorn, base FS non modélisée).
    mu.mem_map(0, 0x1000)
    mu.mem_write(0, struct.pack("<I", 0xFFFFFFFF) + b"\0" * 0x14 + struct.pack("<I", 0) + b"\0" * 0x20)
    mu.mem_write(0x30, struct.pack("<I", 0x1000))
    mu.mem_map(IMAGE_BASE, (size_of_image + 0xFFF) & ~0xFFF)
    mu.mem_write(IMAGE_BASE, data[:hdr])
    for va, vsz, rawptr, rawsz, chars in secs:
        n = min(rawsz, vsz) if vsz else rawsz
        mu.mem_write(IMAGE_BASE + va, data[rawptr:rawptr + n])
    for idx, (iat, name) in enumerate(imps):
        stub = STUB_BASE + 16 * idx
        stub_name[stub] = name
        mu.mem_write(iat, struct.pack("<I", stub))
    mu.mem_map(STUB_BASE, 0x100000, UC_PROT_ALL)
    mu.mem_write(STUB_BASE, b"\xC3" * 0x100000)
    mu.mem_map(0x10000000, 0x400000, UC_PROT_READ | UC_PROT_WRITE)
    mu.mem_map(0x20000000, 0x4000000, UC_PROT_READ | UC_PROT_WRITE)
    mu.mem_map(SENTINEL & ~0xFFF, 0x1000, UC_PROT_ALL)
    mu.mem_write(SENTINEL & ~0xFFF, b"\xC3" * 0x1000)

    mu.reg_write(UC_X86_REG_ESP, STACK_TOP)
    mu.mem_write(STACK_TOP, struct.pack("<I", SENTINEL))

    mods = []
    state = {"count": 0, "heap": HEAP_BASE, "tls": 0, "stop": False, "tlsvals": {}}
    lines = []

    def heap_alloc(size):
        p = state["heap"]
        state["heap"] += (size + 8 + 15) & ~15
        mu.mem_write(p, struct.pack("<I", size))
        return p + 8

    def shim(name, a):
        if name.endswith("HeapAlloc"):
            import os
            if os.environ.get("CT3_DBG") and a[2] in (9, 73):
                g = struct.unpack("<I", bytes(mu.mem_read(0x7dc0d4, 4)))[0]
                print("DBG 0x7dc0d4=%x" % g, file=sys.stderr)
                print("DBG buf=%r" % bytes(mu.mem_read(0x7dba68, 64)), file=sys.stderr)
                if g:
                    print("DBG g=%r" % bytes(mu.mem_read(g, 32)), file=sys.stderr)
            r = heap_alloc(a[2])
            if os.environ.get("CT3_DBG") and a[2] == 0x351:
                print("DBG351 ret=%x cache=%x" % (r, struct.unpack("<I", bytes(mu.mem_read(0x70d39c, 4)))[0]), file=sys.stderr)
            return r
        if name.endswith("GetACP"):
            return 1252
        if name.endswith("GetOEMCP"):
            return 437
        if name.endswith("GetVersion"):
            return 0x0A280105
        if name.endswith("FlsAlloc"):
            state["fls"] = state.get("fls", 0)
            state["fls"] += 1
            return state["fls"] - 1
        if name.endswith("FlsGetValue") or name.endswith("TlsGetValue"):
            return state["tlsvals"].get(("Tls" if "Tls" in name else "Fls", a[0]), 0)
        if name.endswith("FlsSetValue") or name.endswith("TlsSetValue"):
            state["tlsvals"][("Tls" if "Tls" in name else "Fls", a[0])] = a[1]
            return 1
        if name.endswith("TlsAlloc"):
            state["tls"] += 1
            return state["tls"] - 1
        if "Interlocked" in name:
            rd = lambda ad: struct.unpack("<I", bytes(mu.mem_read(ad, 4)))[0]
            wr = lambda ad, v: mu.mem_write(ad, struct.pack("<I", v & 0xFFFFFFFF))
            old = rd(a[0])
            if name.endswith("InterlockedIncrement"):
                wr(a[0], old + 1)
                return old + 1
            if name.endswith("InterlockedDecrement"):
                wr(a[0], old - 1)
                return old - 1
            if name.endswith("InterlockedExchange"):
                wr(a[0], a[1])
                return old
            if name.endswith("InterlockedCompareExchange"):
                if old == a[2]:
                    wr(a[0], a[1])
                return old
        if name.endswith("HeapSize"):
            return struct.unpack("<I", bytes(mu.mem_read(a[2] - 8, 4)))[0]
        if name.endswith("HeapReAlloc"):
            old = struct.unpack("<I", bytes(mu.mem_read(a[2] - 8, 4)))[0]
            q = heap_alloc(a[3])
            mu.mem_write(q, bytes(mu.mem_read(a[2], min(old, a[3]))))
            return q
        if name.endswith("GetProcessHeap"):
            return 0x30000000
        if name.endswith("GetModuleHandleA"):
            if a[0] == 0:
                return IMAGE_BASE
            mod = read_cstr(mu, a[0])[:255].lower()
            if mod not in mods:
                mods.append(mod)
            return 0x60000000 + 16 * mods.index(mod)
        if name.endswith("GetProcAddress"):
            h = a[0]
            if h < 0x60000000 or (h - 0x60000000) // 16 >= len(mods) or (h - 0x60000000) % 16:
                return 0
            return dyn_stub_for(mods[(h - 0x60000000) // 16], a[1])
        if name.endswith("GetStdHandle"):
            return 0x10 + ((-a[0] - 10) & 0xFFFFFFFF)
        if name.endswith("IsValidCodePage") or name.endswith("GetCPInfo"):
            return 1
        if name.endswith("GetVersionExA"):
            mu.mem_write(a[0] + 4, struct.pack("<IIII", 5, 1, 2600, 2))
            mu.mem_write(a[0] + 20, b"\0" * 128)
            return 1
        if name.endswith("GetModuleFileNameA"):
            raw = args.exe.encode()
            n = min(len(raw), a[2] - 1) if a[2] else 0
            if a[2] == 0:
                return 0
            mu.mem_write(a[1], raw[:n] + b"\0")
            return n
        if name.endswith("GetCommandLineA"):
            raw = ('"%s"' % args.exe).encode() + b"\0"
            mu.mem_write(0x10200100, raw)
            return 0x10200100
        if name.endswith("GetEnvironmentStringsW"):
            mu.mem_write(0x10200000, b"\0" * 4)
            return 0x10200000
        if name.endswith("GetFileType"):
            return 2 if a[0] in (0x10, 0x11, 0x12) else 0
        if name.endswith("HeapCreate"):
            return 0x30000001
        if name.endswith("GetTickCount") or name.endswith("timeGetTime"):
            return 1000
        return 0

    dyn = {}

    def dyn_stub_for(mod, name_addr):
        name = read_cstr(mu, name_addr)
        key = "%s::%s" % (mod, name)
        if key not in dyn:
            stub = STUB_BASE + 16 * (len(imps) + len(dyn))
            dyn[key] = stub
            stub_name[stub] = "dyn::" + key
        return dyn[key]

    def read_cstr(uc, addr):
        out = b""
        while True:
            c = bytes(uc.mem_read(addr + len(out), 1))
            if c == b"\0":
                break
            out += c
        return out.decode(errors="replace")

    def hook(uc, addr, size, user):
        if addr == SENTINEL:
            state["stop"] = True
            uc.emu_stop()
            return
        if STUB_BASE <= addr < STUB_BASE + 0x100000 and addr in stub_name:
            name = stub_name[addr]
            short = name.split("::")[-1]
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", bytes(uc.mem_read(esp, 4)))[0]
            if name.startswith("dyn::"):
                short = name.split("::")[-1]
            n = NARGS.get(short)
            if n is None:
                lines.append("[fin] import sans description : %s" % name)
                state["stop"] = True
                uc.emu_stop()
                return
            a = [struct.unpack("<I", bytes(uc.mem_read(esp + 4 + 4 * k, 4)))[0] for k in range(n)]
            state["count"] += 1
            lines.append("[import] (retour 0x%08x) %s(%s)" % (ret, name, ", ".join("0x%x" % x for x in a)))
            if state["count"] >= args.max:
                state["stop"] = True
                uc.emu_stop()
                return
            r = shim_return(short, a, uc)
            if r is None:
                r = shim(short, a)
            uc.reg_write(UC_X86_REG_EAX, r & 0xFFFFFFFF)
            uc.reg_write(UC_X86_REG_ESP, esp + 4 + 4 * n)
            uc.reg_write(UC_X86_REG_EIP, ret)

    mu.hook_add(UC_HOOK_CODE, hook)
    try:
        mu.emu_start(entry, SENTINEL, count=args.insns)
    except Exception as e:
        lines.append("[fin] exception Unicorn : %s à eip=0x%08x" % (e, mu.reg_read(UC_X86_REG_EIP)))
    for line in lines:
        print(line)
    if not state["stop"]:
        print("[fin] limite d'instructions atteinte")


if __name__ == "__main__":
    sys.exit(main())
