#!/usr/bin/env python3
"""Sonde de la référence Unicorn (environnement ~/.venvs/unicorn-ref).

Vérifie quelles instructions MMX, SSE et 3DNow! Unicorn exécute, et avec quelle précision.
Résultats attendus (voir DECISIONS.md, D-024) :
- MMX (PADDSW, PMULLW) et SSE (ADDPS, RCPPS, PMOVMSKB) : exécutés.
- 3DNow! : exécutés seulement avec le modèle UC_CPU_X86_ATHLON (sinon UC_ERR_INSN_INVALID).
- PFRCP : résultat arrondi à la précision float (pas l'approximation matérielle 2^-14).
- Les registres MMX ne sont pas accessibles directement : chargement via MOVQ depuis la mémoire.
"""

import struct

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_CPU_X86_ATHLON, UC_X86_REG_ESP, UC_X86_REG_XMM0, UC_X86_REG_XMM1,
                               UC_X86_REG_ECX, UC_X86_REG_EAX)

BASE = 0x1000
DATA = 0x2000


def machine(model=None):
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    if model is not None:
        mu.ctl_set_cpu_model(model)
    mu.mem_map(BASE, 0x1000)
    mu.mem_map(DATA, 0x1000)
    mu.reg_write(UC_X86_REG_ESP, 0x2F00)
    return mu


def run(mu, code, data):
    mu.mem_write(BASE, code)
    mu.mem_write(DATA, data.ljust(16, b"\0"))
    mu.emu_start(BASE, BASE + len(code))
    return bytes(mu.mem_read(DATA + 0x100, 16))


def movq_load(reg_modrm_disp, addr):
    return bytes([0x0F, 0x6F, reg_modrm_disp]) + addr.to_bytes(4, "little")


def movq_store(addr):
    return bytes([0x0F, 0x7F, 0x05]) + (addr + 0x100).to_bytes(4, "little")


def main():
    # PADDSW mm0, mm1 (saturation signée) : 0F ED C1
    mu = machine()
    code = movq_load(0x05, DATA) + movq_load(0x0D, DATA + 8) + bytes([0x0F, 0xED, 0xC1]) + movq_store(DATA)
    out = run(mu, code, struct.pack("<hhhh", 0x7FFF, 1, 2, -3) + struct.pack("<hhhh", 1, -32768, 2, -5))
    print("PADDSW   ", struct.unpack("<hhhh", out[:8]), "attendu (32767, -32767, 4, -8)")

    # ADDPS xmm0, xmm1 : 0F 58 C1
    mu = machine()
    mu.reg_write(UC_X86_REG_XMM0, int.from_bytes(struct.pack("<ffff", 1, 2, 3, 4), "little"))
    mu.reg_write(UC_X86_REG_XMM1, int.from_bytes(struct.pack("<ffff", 10, 20, 30, 40), "little"))
    mu.mem_write(BASE, bytes([0x0F, 0x58, 0xC1]))
    mu.emu_start(BASE, BASE + 3)
    print("ADDPS    ", struct.unpack("<ffff", mu.reg_read(UC_X86_REG_XMM0).to_bytes(16, "little")),
          "attendu (11, 22, 33, 44)")

    # 3DNow! PFADD mm0, mm1 : 0F 0F C1 9E, modèle Athlon
    mu = machine(UC_CPU_X86_ATHLON)
    code = movq_load(0x05, DATA) + movq_load(0x0D, DATA + 8) + bytes([0x0F, 0x0F, 0xC1, 0x9E]) + movq_store(DATA)
    out = run(mu, code, struct.pack("<ff", 1.0, 2.0) + struct.pack("<ff", 0.5, 0.25))
    print("PFADD    ", struct.unpack("<ff", out[:8]), "attendu (1.5, 2.25) [modèle Athlon]")

    # PFRCP mm0, mm0 : 0F 0F C0 96 ; seule la moitié basse (3.0) est utilisée
    mu = machine(UC_CPU_X86_ATHLON)
    code = movq_load(0x05, DATA) + bytes([0x0F, 0x0F, 0xC0, 0x96]) + movq_store(DATA)
    out = run(mu, code, struct.pack("<ff", 3.0, 0.1))
    val = struct.unpack("<f", out[:4])[0]
    print("PFRCP(3) ", repr(val), "écart relatif %.2e" % abs(val - 1 / 3))


if __name__ == "__main__":
    main()
