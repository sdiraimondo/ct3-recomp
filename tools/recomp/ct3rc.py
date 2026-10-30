#!/usr/bin/env python3
"""Générateur p-code -> C pour la recompilation statique de CT3.exe (x86).

Entrée : JSON produit par tools/ghidra/ExportFunctions.java (mode pcode).
Sortie : gen_funcs.c (une fonction C par routine guest) et gen_dispatch.c
(répartition des appels par adresse guest). Le code généré dépend de
rt.h ; il ne contient aucun octet du jeu.

Conventions du code généré :
- Registres et drapeaux : tableau d'octets cpu_t.reg indexé par l'offset
  p-code du registre (rd/wr little-endian, sous-registres inclus).
- Temporaires unique : variables locales uint64_t uN (valeurs masquées).
- Appels et sauts hors fonction : dispatch(c, adresse_guest).
"""

import argparse
import json
import sys
from collections import Counter

MASK_BITS = {1: 0xFF, 2: 0xFFFF, 4: 0xFFFFFFFF, 8: 0xFFFFFFFFFFFFFFFF}


def mask(size):
    return MASK_BITS[size]


class Unsupported(Exception):
    pass


class FunctionTranslator:
    def __init__(self, fn, stats):
        self.fn = fn
        self.name = fn["name"]
        self.entry = int(fn["entry"], 16)
        self.stats = stats
        self.uniques = set()
        self.insn_addrs = {int(i["addr"], 16) for i in fn["insns"]}
        self.ram_labels = set()
        self.op_labels = set()
        self.lines = []

    def cname(self):
        return "sub_%08x" % self.entry

    # ---- expressions -------------------------------------------------
    def val(self, v):
        s, o, z = v["s"], v["o"], v["z"]
        if z > 8:
            raise Unsupported("varnode de taille %d" % z)
        if s == "const":
            return "0x%xULL" % (o & mask(z))
        if s == "register":
            return "cpu_rd(c, %d, %d)" % (o, z)
        if s == "unique":
            self.uniques.add(o)
            return "(u%d & 0x%xULL)" % (o, mask(z))
        if s == "ram":
            return "guest_ld(c, 0x%xU, %d)" % (o & 0xFFFFFFFF, z)
        raise Unsupported("espace %s en entrée" % s)

    def assign(self, out, expr):
        s, o, z = out["s"], out["o"], out["z"]
        if s == "register":
            return "cpu_wr(c, %d, %d, (%s));" % (o, z, expr)
        if s == "unique":
            self.uniques.add(o)
            return "u%d = (%s) & 0x%xULL;" % (o, expr, mask(z))
        if s == "ram":
            return "guest_st(c, 0x%xU, %d, (%s));" % (o & 0xFFFFFFFF, z, expr)
        raise Unsupported("espace %s en sortie" % s)

    @staticmethod
    def sx(expr, size):
        if size == 8:
            return "((int64_t)(%s))" % expr
        sh = 64 - 8 * size
        return "((int64_t)((%s) << %d) >> %d)" % (expr, sh, sh)

    # ---- operations ------------------------------------------------
    def emit_op(self, idx, op):
        m = op["op"]
        ins = op["in"]
        out = op["out"]
        osz = out["z"] if out else 0
        M = "0x%xULL" % mask(osz) if osz else "0"

        def a(i):
            return self.val(ins[i])

        def asz(i):
            return ins[i]["z"]

        if m == "COPY":
            return self.assign(out, a(0))
        if m == "LOAD":
            return self.assign(out, "guest_ld(c, (uint32_t)%s, %d)" % (a(1), osz))
        if m == "STORE":
            return "guest_st(c, (uint32_t)%s, %d, %s);" % (a(1), asz(2), a(2))
        if m == "INT_ADD":
            return self.assign(out, "%s + %s" % (a(0), a(1)))
        if m == "INT_SUB":
            return self.assign(out, "%s - %s" % (a(0), a(1)))
        if m == "INT_MULT":
            return self.assign(out, "%s * %s" % (a(0), a(1)))
        if m == "INT_AND":
            return self.assign(out, "%s & %s" % (a(0), a(1)))
        if m == "INT_OR":
            return self.assign(out, "%s | %s" % (a(0), a(1)))
        if m == "INT_XOR":
            return self.assign(out, "%s ^ %s" % (a(0), a(1)))
        if m == "INT_2COMP":
            return self.assign(out, "0 - %s" % a(0))
        if m == "INT_NEGATE":
            return self.assign(out, "~%s" % a(0))
        if m == "INT_LEFT":
            return self.assign(out, "(%s >= 64) ? 0 : (%s << %s)" % (a(1), a(0), a(1)))
        if m == "INT_RIGHT":
            return self.assign(out, "(%s >= 64) ? 0 : (%s >> %s)" % (a(1), a(0), a(1)))
        if m == "INT_SRIGHT":
            sa = asz(0)
            return self.assign(
                out,
                "(%s >= 64) ? (uint64_t)(%s < 0 ? -1LL : 0) : (uint64_t)(%s >> %s)"
                % (a(1), self.sx(a(0), sa), self.sx(a(0), sa), a(1)),
            )
        if m == "INT_DIV":
            return self.assign(out, "rt_div_u(%s, %s)" % (a(0), a(1)))
        if m == "INT_REM":
            return self.assign(out, "rt_rem_u(%s, %s)" % (a(0), a(1)))
        if m == "INT_SDIV":
            sz = asz(0)
            return self.assign(out, "(uint64_t)rt_div_s(%s, %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_SREM":
            sz = asz(0)
            return self.assign(out, "(uint64_t)rt_rem_s(%s, %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_EQUAL":
            return self.assign(out, "(%s == %s)" % (a(0), a(1)))
        if m == "INT_NOTEQUAL":
            return self.assign(out, "(%s != %s)" % (a(0), a(1)))
        if m == "INT_LESS":
            return self.assign(out, "(%s < %s)" % (a(0), a(1)))
        if m == "INT_LESSEQUAL":
            return self.assign(out, "(%s <= %s)" % (a(0), a(1)))
        if m == "INT_SLESS":
            sz = asz(0)
            return self.assign(out, "(%s < %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_SLESSEQUAL":
            sz = asz(0)
            return self.assign(out, "(%s <= %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_CARRY":
            sz = asz(0)
            return self.assign(out, "rt_carry_add(%s, %s, %d)" % (a(0), a(1), sz))
        if m == "INT_SCARRY":
            sz = asz(0)
            return self.assign(out, "rt_scarry_add(%s, %s, %d)" % (a(0), a(1), sz))
        if m == "INT_SBORROW":
            sz = asz(0)
            return self.assign(out, "rt_sborrow_sub(%s, %s, %d)" % (a(0), a(1), sz))
        if m == "INT_ZEXT":
            return self.assign(out, a(0))
        if m == "INT_SEXT":
            sz = asz(0)
            return self.assign(out, "(uint64_t)%s" % self.sx(a(0), sz))
        if m == "POPCOUNT":
            return self.assign(out, "(uint64_t)__builtin_popcountll(%s)" % a(0))
        if m == "BOOL_NEGATE":
            return self.assign(out, "(%s == 0)" % a(0))
        if m == "BOOL_AND":
            return self.assign(out, "(%s & %s)" % (a(0), a(1)))
        if m == "BOOL_OR":
            return self.assign(out, "(%s | %s)" % (a(0), a(1)))
        if m == "BOOL_XOR":
            return self.assign(out, "(%s ^ %s)" % (a(0), a(1)))
        if m == "SUBPIECE":
            k = ins[1]["o"]
            return self.assign(out, "(%s >> %d)" % (a(0), 8 * k))
        if m == "PIECE":
            lo_sz = asz(1)
            return self.assign(out, "((%s << %d) | %s)" % (a(0), 8 * lo_sz, a(1)))
        if m == "CALL":
            tgt = int(ins[0]["o"])
            return "dispatch(c, 0x%x);" % tgt
        if m == "CALLIND":
            return "dispatch(c, (uint32_t)%s);" % a(0)
        if m == "RETURN":
            return "return;"
        if m == "BRANCH":
            tgt = ins[0]
            if tgt["s"] == "const":
                self.op_labels.add((self.cur_addr, tgt["o"]))
                return "goto P_%08x_%d;" % (self.cur_addr, tgt["o"])
            t = tgt["o"]
            if t in self.insn_addrs:
                self.ram_labels.add(t)
                return "goto A_%08x;" % t
            return "dispatch(c, 0x%x); return;" % t
        if m == "CBRANCH":
            tgt = ins[0]
            cond = a(1)
            if tgt["s"] == "const":
                self.op_labels.add((self.cur_addr, tgt["o"]))
                return "if (%s) goto P_%08x_%d;" % (cond, self.cur_addr, tgt["o"])
            t = tgt["o"]
            if t in self.insn_addrs:
                self.ram_labels.add(t)
                return "if (%s) goto A_%08x;" % (cond, t)
            return "if (%s) { dispatch(c, 0x%x); return; }" % (cond, t)
        raise Unsupported(m)

    # ---- fonction -----------------------------------------------------
    def translate(self):
        body = []
        for ins in self.fn["insns"]:
            self.cur_addr = int(ins["addr"], 16)
            ops = []
            for idx, op in enumerate(ins["pcode"]):
                try:
                    code = self.emit_op(idx, op)
                except Unsupported as e:
                    self.stats["unsupported"][str(e)] += 1
                    self.stats["unsupported_sites"].append((self.name, ins["addr"], str(e)))
                    code = 'rt_unsupported("%s", 0x%x);' % (e, self.cur_addr)
                ops.append((idx, code))
            body.append((self.cur_addr, ins, ops))
        out = []
        out.append("void %s(cpu_t *c)" % self.cname())
        out.append("{")
        decl = sorted(self.uniques)
        if decl:
            out.append("    uint64_t " + ", ".join("u%d = 0" % o for o in decl) + ";")
        for addr, ins, ops in body:
            if addr in self.ram_labels:
                out.append("A_%08x:;" % addr)
            for idx, code in ops:
                if (addr, idx) in self.op_labels:
                    out.append("P_%08x_%d:;" % (addr, idx))
                out.append("    " + code + "  /* %s */" % ins["text"].replace("*/", "*\\/"))
        out.append("    return;")
        out.append("}")
        out.append("")
        return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pcode_json")
    ap.add_argument("outdir")
    args = ap.parse_args()

    with open(args.pcode_json) as f:
        functions = json.load(f)

    stats = {"unsupported": Counter(), "unsupported_sites": []}
    chunks = [
        "/* Généré par tools/recomp/ct3rc.py : ne pas modifier. */",
        '#include "rt.h"',
        "",
    ]
    entries = []
    for fn in functions:
        t = FunctionTranslator(fn, stats)
        chunks.append(t.translate())
        entries.append((t.entry, t.cname()))

    import os

    os.makedirs(args.outdir, exist_ok=True)
    with open(os.path.join(args.outdir, "gen_funcs.c"), "w") as f:
        f.write("\n".join(chunks))
    with open(os.path.join(args.outdir, "gen_funcs.h"), "w") as f:
        f.write("/* Généré par tools/recomp/ct3rc.py */\n#pragma once\n#include \"rt.h\"\n")
        for _, cname in entries:
            f.write("void %s(cpu_t *c);\n" % cname)
    with open(os.path.join(args.outdir, "gen_dispatch.c"), "w") as f:
        f.write("/* Généré par tools/recomp/ct3rc.py : ne pas modifier. */\n")
        f.write('#include "rt.h"\n#include "gen_funcs.h"\n\n')
        f.write("void dispatch(cpu_t *c, uint32_t addr)\n{\n")
        f.write("    if (addr >= RT_STUB_BASE && addr < RT_STUB_END) {\n")
        f.write("        rt_shim_call(c, addr);\n        return;\n    }\n")
        f.write("    switch (addr) {\n")
        for entry, cname in sorted(entries):
            f.write("    case 0x%08x: %s(c); return;\n" % (entry, cname))
        f.write("    default: rt_unresolved(c, addr);\n    }\n}\n")

    total = len(stats["unsupported_sites"])
    print("fonctions traduites : %d" % len(entries))
    print("sites non pris en charge : %d" % total)
    for k, v in stats["unsupported"].most_common():
        print("  %-20s %d" % (k, v))
    return 0


if __name__ == "__main__":
    sys.exit(main())
