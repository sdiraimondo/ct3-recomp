#!/usr/bin/env python3
"""Génère des vecteurs de test (entrées/sorties) à partir de Unicorn, pour les opérations
MMX, SSE et 3DNow! du runtime. Sortie : tests/vec/vectors.h.

Chaque opération est exécutée dans Unicorn (modèle Athlon) sur des entrées aléatoires
(graine fixe) et sur des cas limites. Les entrées sont chargées par MOVQ/MOVUPS depuis
la mémoire, le résultat est relu de la même façon.
"""

import argparse
import os
import random
import struct

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_CPU_X86_ATHLON, UC_X86_REG_ESP

BASE = 0x1000
DATA = 0x2000

# name, taille (8 ou 16), encodage (bytes après le chargement de mm0/xmm0 et mm1/xmm1), arité
# Les deux opérandes sont mm0/xmm0 (destination) et mm1/xmm1 (source) : ModRM C1.
MMX_OPS = [
    ("PACKSSWB", 8, bytes([0x0F, 0x63, 0xC1]), 2),
    ("PACKSSDW", 8, bytes([0x0F, 0x6B, 0xC1]), 2),
    ("PADDSW", 8, bytes([0x0F, 0xED, 0xC1]), 2),
    ("PSUBSW", 8, bytes([0x0F, 0xE9, 0xC1]), 2),
    ("PMADDWD", 8, bytes([0x0F, 0xF5, 0xC1]), 2),
    ("PMULHW", 8, bytes([0x0F, 0xE5, 0xC1]), 2),
    ("PSLLW", 8, bytes([0x0F, 0xF1, 0xC1]), 2),
    ("PSRAW", 8, bytes([0x0F, 0xE1, 0xC1]), 2),
]

THREEDNOW_OPS = [
    ("PFADD", 8, bytes([0x0F, 0x0F, 0xC1, 0x9E]), 2),
    ("PFSUB", 8, bytes([0x0F, 0x0F, 0xC1, 0x9A]), 2),
    ("PFSUBR", 8, bytes([0x0F, 0x0F, 0xC1, 0xAA]), 2),
    ("PFMUL", 8, bytes([0x0F, 0x0F, 0xC1, 0xB4]), 2),
    ("PFMIN", 8, bytes([0x0F, 0x0F, 0xC1, 0x94]), 2),
    ("PFMAX", 8, bytes([0x0F, 0x0F, 0xC1, 0xA4]), 2),
    ("PFCMPGE", 8, bytes([0x0F, 0x0F, 0xC1, 0x90]), 2),
    ("PFCMPGT", 8, bytes([0x0F, 0x0F, 0xC1, 0xA0]), 2),
    ("PFCMPEQ", 8, bytes([0x0F, 0x0F, 0xC1, 0xB0]), 2),
    ("PFACC", 8, bytes([0x0F, 0x0F, 0xC1, 0xAE]), 2),
    ("PFNACC", 8, bytes([0x0F, 0x0F, 0xC1, 0x8A]), 2),
    ("PSWAPD", 8, bytes([0x0F, 0x0F, 0xC1, 0xBB]), 2),
    ("PI2FD", 8, bytes([0x0F, 0x0F, 0xC1, 0x0D]), 2),
    ("PF2ID", 8, bytes([0x0F, 0x0F, 0xC1, 0x1D]), 2),
    ("PFRCP", 8, bytes([0x0F, 0x0F, 0xC1, 0x96]), 2),
    ("PFRSQRT", 8, bytes([0x0F, 0x0F, 0xC1, 0x97]), 2),
    ("PFRCPIT1", 8, bytes([0x0F, 0x0F, 0xC1, 0xA6]), 2),
    ("PFRSQIT1", 8, bytes([0x0F, 0x0F, 0xC1, 0xA7]), 2),
    ("PFRCPIT2", 8, bytes([0x0F, 0x0F, 0xC1, 0xB6]), 2),
]

SSE_OPS = [
    ("RCPPS", 16, bytes([0x0F, 0x53, 0xC1]), 2),
    ("RCPSS", 16, bytes([0xF3, 0x0F, 0x53, 0xC1]), 2),
    ("RSQRTSS", 16, bytes([0xF3, 0x0F, 0x52, 0xC1]), 2),
]


def machine(athlon=True):
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    if athlon:
        mu.ctl_set_cpu_model(UC_CPU_X86_ATHLON)
    mu.mem_map(BASE, 0x1000)
    mu.mem_map(DATA, 0x1000)
    mu.reg_write(UC_X86_REG_ESP, 0x2F00)
    return mu


def execute(mu, size, op, a, b):
    """Retourne les `size` octets de la destination après exécution de op(a, b)."""
    if size == 8:
        load_a = bytes([0x0F, 0x6F, 0x05]) + DATA.to_bytes(4, "little")
        load_b = bytes([0x0F, 0x6F, 0x0D]) + (DATA + 8).to_bytes(4, "little")
        store = bytes([0x0F, 0x7F, 0x05]) + (DATA + 0x100).to_bytes(4, "little")
    else:
        load_a = bytes([0x0F, 0x10, 0x05]) + DATA.to_bytes(4, "little")
        load_b = bytes([0x0F, 0x10, 0x0D]) + (DATA + 16).to_bytes(4, "little")
        store = bytes([0x0F, 0x11, 0x05]) + (DATA + 0x100).to_bytes(4, "little")
    code = load_a + load_b + op + store
    mu.mem_write(BASE, code)
    mu.mem_write(DATA, (a + b).ljust(32, b"\0"))
    mu.mem_write(DATA + 0x100, b"\0" * 16)
    mu.emu_start(BASE, BASE + len(code))
    return bytes(mu.mem_read(DATA + 0x100, size))


def word_edges(rng):
    vals = [0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x7FFE, 0x8001, 0x0100, 0xFF00, 0x4000]
    return vals


def random_bytes(rng, size, mode):
    out = bytearray()
    for i in range(size // 2):
        if mode == "edge" and rng.random() < 0.5:
            v = rng.choice(word_edges(rng))
        else:
            v = rng.randrange(0x10000)
        out += struct.pack("<H", v)
    return bytes(out)


def random_floats(rng, size, mode):
    out = bytearray()
    for i in range(size // 4):
        if mode == "edge" and rng.random() < 0.3:
            v = rng.choice([0.0, 1.0, -1.0, 0.5, 2.0, 1e-3, 1e3, -2.5])
        else:
            v = rng.uniform(-100.0, 100.0)
            if rng.random() < 0.2:
                v = rng.uniform(-1e-3, 1e-3)
        out += struct.pack("<f", v)
    return bytes(out)


def vectors_for(name, size, rng, count):
    floaty = name.startswith(("PF", "RCP", "RSQRT")) and name not in ("PFACC", "PFNACC", "PSWAPD")
    floaty = floaty or name in ("PI2FD", "PF2ID")
    gen = random_floats if floaty else random_bytes
    vecs = []
    for k in range(count):
        mode = "edge" if k % 2 == 0 else "rand"
        vecs.append((gen(rng, size, mode).ljust(16, b"\0"), gen(rng, size, mode).ljust(16, b"\0")))
    return vecs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", help="fichier d'en-tête généré")
    ap.add_argument("--count", type=int, default=400)
    args = ap.parse_args()

    rng = random.Random(20261031)
    lines = [
        "/* Généré par tools/ref/gen_vectors.py (Unicorn, modèle Athlon) : ne pas modifier. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
    ]
    index = []
    for group, ops in (("mmx", MMX_OPS), ("3dnow", THREEDNOW_OPS), ("sse", SSE_OPS)):
        # SSE : le modèle Athlon n'a pas SSE ; le modèle par défaut d'Unicorn l'exécute.
        mu = machine(athlon=(group != "sse"))
        for name, size, op, arity in ops:
            vecs = vectors_for(name, size, rng, args.count)
            rows = []
            for a, b in vecs:
                out = execute(mu, size, op, a[:size], b[:size])
                rows.append((a, b, out))
            lines.append("static const struct { uint8_t a[16]; uint8_t b[16]; uint8_t out[16]; } "
                         "vec_%s[] = {" % name)
            for a, b, out in rows:
                lines.append("    {{%s}, {%s}, {%s}}," % (
                    ", ".join("0x%02x" % x for x in a),
                    ", ".join("0x%02x" % x for x in b),
                    ", ".join("0x%02x" % x for x in out.ljust(16, b"\0"))))
            lines.append("};")
            lines.append("#define VEC_COUNT_%s %d" % (name, len(rows)))
            lines.append("#define VEC_SIZE_%s %d" % (name, size))
            lines.append("")
            index.append((group, name, size, len(rows)))
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    for group, name, size, n in index:
        print("%-6s %-10s taille %2d, %d vecteurs" % (group, name, size, n))


if __name__ == "__main__":
    main()
