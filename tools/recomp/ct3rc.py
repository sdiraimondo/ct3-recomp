#!/usr/bin/env python3
"""Générateur p-code -> C pour la recompilation statique de CT3.exe (x86).

Entrée : JSON produit par tools/ghidra/ExportFunctions.java (modes pcode, pcode-all).
Sortie : gen_funcs_NNN.c (une fonction C par routine guest), gen_funcs.h,
gen_dispatch.c, rapport.md. Le code généré dépend de rt.h et rt_fp.h.

Conventions du code généré :
- Registres et drapeaux : tableau d'octets cpu_t.reg indexé par l'offset p-code.
- Entiers (<= 8 octets) : variables locales uint64_t, valeurs masquées.
- Flottants x87 (10 octets) et autres valeurs > 8 octets : accès par pointeur
  (registre, temporaire, mémoire guest), opérations via rt_fp.h.
- Appels et sauts hors fonction : dispatch(c, adresse_guest).
"""

import argparse
import json
import os
import sys
from collections import Counter

MASK_BITS = {1: 0xFF, 2: 0xFFFF, 4: 0xFFFFFFFF, 8: 0xFFFFFFFFFFFFFFFF}

# Userops CALLOTHER observés dans CT3.exe (index p-code -> expression sur les entrées).
# a = premier opérande flottant après l'index, b = second. Vérifié sur les instructions
# FSIN/FCOS/F2XM1/FPATAN/FYL2X/FSINCOS du binaire.
X87_USEROPS = {
    105: ("F2XM1", 1, "exp2l({a}) - 1.0L"),
    108: ("FCOS", 1, "cosl({a})"),
    114: ("FSIN", 1, "sinl({a})"),
    111: ("FPATAN", 2, "atan2l({a}, {b})"),
    119: ("FYL2X", 1, "log2l({a})"),
    112: ("FPTAN", 1, "tanl({a})"),
    113: ("FSCALE", 2, "{a} * exp2l(truncl({b}))"),
}


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
        self.ints = set()
        self.wides = set()
        self.insn_addrs = {int(i["addr"], 16) for i in fn["insns"]}
        self.ram_labels = set()
        self.op_labels = set()
        self.callees = set()
        self.indirect = 0
        self.cur_addr = 0
        self.cur_targets = []
        self.cur_is_last = False

    def cname(self):
        return "sub_%08x" % self.entry

    # ---- varnodes ----------------------------------------------------
    def uvar(self, o, z):
        if z > 8:
            self.wides.add((o, z))
            return "w%d_%d" % (o, z)
        self.ints.add((o, z))
        return "u%d_%d" % (o, z)

    def int_val(self, v):
        s, o, z = v["s"], v["o"], v["z"]
        if z > 8:
            raise Unsupported("valeur de %d octets en contexte entier" % z)
        if s == "const":
            return "0x%xULL" % (o & mask(z))
        if s == "register":
            return "cpu_rd(c, %d, %d)" % (o, z)
        if s == "unique":
            return "(%s & 0x%xULL)" % (self.uvar(o, z), mask(z))
        if s == "ram":
            return "guest_ld(c, 0x%xU, %d)" % (o & 0xFFFFFFFF, z)
        raise Unsupported("espace %s en entrée" % s)

    def ptr(self, v):
        """Pointeur vers les octets d'une valeur (utilisé pour x87 et les valeurs > 8 octets)."""
        s, o, z = v["s"], v["o"], v["z"]
        if s == "register":
            return "(c->reg + %d)" % o
        if s == "unique":
            return "((uint8_t *)&%s)" % self.uvar(o, z) if z <= 8 else "(%s.b)" % self.uvar(o, z)
        if s == "ram":
            return "(rt_mem_base + 0x%xU)" % (o & 0xFFFFFFFF)
        if s == "const":
            if z > 8:
                raise Unsupported("constante de %d octets" % z)
            byts = ", ".join("0x%x" % ((o >> (8 * i)) & 0xFF) for i in range(z))
            return "((const uint8_t[]){%s})" % byts
        raise Unsupported("espace %s en pointeur" % s)

    def int_assign(self, out, expr):
        s, o, z = out["s"], out["o"], out["z"]
        if z > 8:
            raise Unsupported("sortie entière de %d octets" % z)
        if s == "register":
            return "cpu_wr(c, %d, %d, (%s));" % (o, z, expr)
        if s == "unique":
            return "%s = (%s) & 0x%xULL;" % (self.uvar(o, z), expr, mask(z))
        if s == "ram":
            return "guest_st(c, 0x%xU, %d, (%s));" % (o & 0xFFFFFFFF, z, expr)
        raise Unsupported("espace %s en sortie" % s)

    @staticmethod
    def sx(expr, size):
        if size == 8:
            return "((int64_t)(%s))" % expr
        sh = 64 - 8 * size
        return "((int64_t)((%s) << %d) >> %d)" % (expr, sh, sh)

    def is_wide(self, v):
        return v is not None and v["z"] > 8

    # ---- opérations ---------------------------------------------------
    def emit_op(self, op):
        m = op["op"]
        ins = op["in"]
        out = op["out"]
        osz = out["z"] if out else 0

        def a(i):
            return self.int_val(ins[i])

        def asz(i):
            return ins[i]["z"]

        # Opérations flottantes et valeurs > 8 octets.
        if m.startswith("FLOAT") or m in ("ROUND", "TRUNC", "INT2FLOAT"):
            return self.emit_float(m, out, ins)
        if m == "CALLOTHER":
            return self.emit_callother(out, ins)
        if m == "COPY" and (self.is_wide(out) or self.is_wide(ins[0])):
            if osz <= 8 or asz(0) <= 8:
                raise Unsupported("COPY entre tailles %d et %d" % (osz, asz(0)))
            return "memcpy(%s, %s, %d);" % (self.ptr(out), self.ptr(ins[0]), min(osz, asz(0)))
        if m == "LOAD" and osz > 8:
            return "memcpy(%s, rt_mem_base + (uint32_t)%s, %d);" % (self.ptr(out), self.int_val(ins[1]), osz)
        if m == "STORE" and asz(2) > 8:
            return "memcpy(rt_mem_base + (uint32_t)%s, %s, %d);" % (self.int_val(ins[1]), self.ptr(ins[2]), asz(2))
        if m == "SUBPIECE" and asz(0) > 8:
            k = ins[1]["o"]
            if osz > 8:
                return "memcpy(%s, %s + %d, %d);" % (self.ptr(out), self.ptr(ins[0]), k, osz)
            return self.int_assign(out, "rt_load_bytes(%s + %d, %d)" % (self.ptr(ins[0]), k, osz))

        if m == "COPY":
            return self.int_assign(out, a(0))
        if m == "LOAD":
            return self.int_assign(out, "guest_ld(c, (uint32_t)%s, %d)" % (a(1), osz))
        if m == "STORE":
            return "guest_st(c, (uint32_t)%s, %d, %s);" % (a(1), asz(2), a(2))
        if m == "INT_ADD":
            return self.int_assign(out, "%s + %s" % (a(0), a(1)))
        if m == "INT_SUB":
            return self.int_assign(out, "%s - %s" % (a(0), a(1)))
        if m == "INT_MULT":
            return self.int_assign(out, "%s * %s" % (a(0), a(1)))
        if m == "INT_AND":
            return self.int_assign(out, "%s & %s" % (a(0), a(1)))
        if m == "INT_OR":
            return self.int_assign(out, "%s | %s" % (a(0), a(1)))
        if m == "INT_XOR":
            return self.int_assign(out, "%s ^ %s" % (a(0), a(1)))
        if m == "INT_2COMP":
            return self.int_assign(out, "0 - %s" % a(0))
        if m == "INT_NEGATE":
            return self.int_assign(out, "~%s" % a(0))
        if m == "INT_LEFT":
            return self.int_assign(out, "(%s >= 64) ? 0 : (%s << %s)" % (a(1), a(0), a(1)))
        if m == "INT_RIGHT":
            return self.int_assign(out, "(%s >= 64) ? 0 : (%s >> %s)" % (a(1), a(0), a(1)))
        if m == "INT_SRIGHT":
            sa = asz(0)
            return self.int_assign(
                out,
                "(%s >= 64) ? (uint64_t)(%s < 0 ? -1LL : 0) : (uint64_t)(%s >> %s)"
                % (a(1), self.sx(a(0), sa), self.sx(a(0), sa), a(1)),
            )
        if m == "INT_DIV":
            return self.int_assign(out, "rt_div_u(%s, %s)" % (a(0), a(1)))
        if m == "INT_REM":
            return self.int_assign(out, "rt_rem_u(%s, %s)" % (a(0), a(1)))
        if m == "INT_SDIV":
            sz = asz(0)
            return self.int_assign(out, "(uint64_t)rt_div_s(%s, %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_SREM":
            sz = asz(0)
            return self.int_assign(out, "(uint64_t)rt_rem_s(%s, %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_EQUAL":
            return self.int_assign(out, "(%s == %s)" % (a(0), a(1)))
        if m == "INT_NOTEQUAL":
            return self.int_assign(out, "(%s != %s)" % (a(0), a(1)))
        if m == "INT_LESS":
            return self.int_assign(out, "(%s < %s)" % (a(0), a(1)))
        if m == "INT_LESSEQUAL":
            return self.int_assign(out, "(%s <= %s)" % (a(0), a(1)))
        if m == "INT_SLESS":
            sz = asz(0)
            return self.int_assign(out, "(%s < %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_SLESSEQUAL":
            sz = asz(0)
            return self.int_assign(out, "(%s <= %s)" % (self.sx(a(0), sz), self.sx(a(1), sz)))
        if m == "INT_CARRY":
            return self.int_assign(out, "rt_carry_add(%s, %s, %d)" % (a(0), a(1), asz(0)))
        if m == "INT_SCARRY":
            return self.int_assign(out, "rt_scarry_add(%s, %s, %d)" % (a(0), a(1), asz(0)))
        if m == "INT_SBORROW":
            return self.int_assign(out, "rt_sborrow_sub(%s, %s, %d)" % (a(0), a(1), asz(0)))
        if m == "INT_ZEXT":
            return self.int_assign(out, a(0))
        if m == "INT_SEXT":
            return self.int_assign(out, "(uint64_t)%s" % self.sx(a(0), asz(0)))
        if m == "POPCOUNT":
            return self.int_assign(out, "(uint64_t)__builtin_popcountll(%s)" % a(0))
        if m == "BOOL_NEGATE":
            return self.int_assign(out, "(%s == 0)" % a(0))
        if m == "BOOL_AND":
            return self.int_assign(out, "(%s & %s)" % (a(0), a(1)))
        if m == "BOOL_OR":
            return self.int_assign(out, "(%s | %s)" % (a(0), a(1)))
        if m == "BOOL_XOR":
            return self.int_assign(out, "(%s ^ %s)" % (a(0), a(1)))
        if m == "SUBPIECE":
            k = ins[1]["o"]
            return self.int_assign(out, "(%s >> %d)" % (a(0), 8 * k))
        if m == "PIECE":
            return self.int_assign(out, "((%s << %d) | %s)" % (a(0), 8 * asz(1), a(1)))
        if m == "CALL":
            tgt = int(ins[0]["o"])
            self.callees.add(tgt)
            return "dispatch(c, 0x%x);" % tgt
        if m == "CALLIND":
            self.indirect += 1
            return "dispatch(c, (uint32_t)%s);" % a(0)
        if m == "RETURN":
            return "return;"
        if m == "BRANCHIND":
            self.indirect += 1
            targets = self.cur_targets
            if not targets:
                if not self.cur_is_last:
                    raise Unsupported(m)
                return "dispatch(c, (uint32_t)%s); return;" % a(0)
            parts = ["{ uint32_t t_ = (uint32_t)%s;" % a(0)]
            for t in targets:
                if t in self.insn_addrs:
                    self.ram_labels.add(t)
                    parts.append("if (t_ == 0x%x) goto A_%08x;" % (t, t))
                else:
                    self.callees.add(t)
                    parts.append("if (t_ == 0x%x) { dispatch(c, t_); return; }" % t)
            parts.append('rt_unsupported("saut indirect hors cibles connues", 0x%x); }' % self.cur_addr)
            return " ".join(parts)
        if m == "BRANCH":
            tgt = ins[0]
            if tgt["s"] == "const":
                self.op_labels.add((self.cur_addr, tgt["o"]))
                return "goto P_%08x_%d;" % (self.cur_addr, tgt["o"])
            t = tgt["o"]
            if t in self.insn_addrs:
                self.ram_labels.add(t)
                return "goto A_%08x;" % t
            self.callees.add(t)
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
            self.callees.add(t)
            return "if (%s) { dispatch(c, 0x%x); return; }" % (cond, t)
        raise Unsupported(m)

    def emit_float(self, m, out, ins):
        osz = out["z"] if out else 0
        if m in ("FLOAT_ADD", "FLOAT_SUB", "FLOAT_MULT", "FLOAT_DIV"):
            op = {"FLOAT_ADD": "+", "FLOAT_SUB": "-", "FLOAT_MULT": "*", "FLOAT_DIV": "/"}[m]
            a = "rt_fget(%s, %d)" % (self.ptr(ins[0]), ins[0]["z"])
            b = "rt_fget(%s, %d)" % (self.ptr(ins[1]), ins[1]["z"])
            return "rt_fput(%s, %d, %s %s %s);" % (self.ptr(out), osz, a, op, b)
        if m == "FLOAT_NEG":
            return "rt_fput(%s, %d, -rt_fget(%s, %d));" % (self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        if m == "FLOAT_ABS":
            return "rt_fput(%s, %d, fabsl(rt_fget(%s, %d)));" % (self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        if m == "FLOAT_SQRT":
            return "rt_fput(%s, %d, sqrtl(rt_fget(%s, %d)));" % (self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        if m in ("FLOAT_LESS", "FLOAT_LESSEQUAL", "FLOAT_EQUAL", "FLOAT_NOTEQUAL"):
            fn = {"FLOAT_LESS": "rt_fcmp_lt", "FLOAT_LESSEQUAL": "rt_fcmp_le",
                  "FLOAT_EQUAL": "rt_fcmp_eq", "FLOAT_NOTEQUAL": "rt_fcmp_ne"}[m]
            a = "rt_fget(%s, %d)" % (self.ptr(ins[0]), ins[0]["z"])
            b = "rt_fget(%s, %d)" % (self.ptr(ins[1]), ins[1]["z"])
            return self.int_assign(out, "%s(%s, %s)" % (fn, a, b))
        if m == "FLOAT_NAN":
            return self.int_assign(out, "rt_fcmp_nan(rt_fget(%s, %d))" % (self.ptr(ins[0]), ins[0]["z"]))
        if m == "FLOAT2FLOAT":
            return "rt_fput(%s, %d, rt_fget(%s, %d));" % (self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        if m == "INT2FLOAT":
            src = self.sx(self.int_val(ins[0]), ins[0]["z"])
            return "rt_fput(%s, %d, (long double)%s);" % (self.ptr(out), osz, src)
        if m == "TRUNC":
            return self.int_assign(out, "rt_ftrunc(%s, %d, %d)" % (self.ptr(ins[0]), ins[0]["z"], osz))
        if m == "ROUND":
            return "rt_fput(%s, %d, rt_fround(c, rt_fget(%s, %d)));" % (
                self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        raise Unsupported(m)

    def emit_callother(self, out, ins):
        idx = ins[0]["o"]
        if idx == 16:
            return "rt_int3(0x%x);" % self.cur_addr
        if idx == 76:
            return self.int_assign(out, "rt_rdtsc()")
        if 45 <= idx <= 61:
            return self.int_assign(out, "rt_cpuid_block(c)")
        if idx == 109:
            i = (ins[1]["o"] - 4352) // 16
            return self.int_assign(out, "cpu_rd(c, %d, %d) | (3ULL << %d)" % (out["o"], out["z"], 2 * i))
        spec = X87_USEROPS.get(idx)
        if spec is None:
            raise Unsupported("CALLOTHER %d" % idx)
        name, arity, expr = spec
        args = ins[1:]
        if len(args) != arity:
            raise Unsupported("CALLOTHER %s : %d entrées" % (name, len(args)))
        vals = {}
        for key, v in zip(("a", "b"), args):
            vals[key] = "rt_fget(%s, %d)" % (self.ptr(v), v["z"])
        return "rt_fput(%s, %d, %s);" % (self.ptr(out), out["z"], expr.format(**vals))

    # ---- fonction -----------------------------------------------------
    def translate(self):
        body = []
        last = len(self.fn["insns"]) - 1
        for n, ins in enumerate(self.fn["insns"]):
            self.cur_addr = int(ins["addr"], 16)
            self.cur_targets = [int(t, 16) for t in ins.get("targets", []) if is_hex_addr(t)]
            self.cur_is_last = n == last
            ops = []
            for idx, op in enumerate(ins["pcode"]):
                try:
                    code = self.emit_op(op)
                except Unsupported as e:
                    self.stats["unsupported"][str(e)] += 1
                    mnem = ins["text"].split(" ")[0]
                    self.stats["unsupported_sites"].append((self.name, ins["addr"], mnem))
                    self.stats["unsupported_fn_entries"].add(self.entry)
                    code = 'rt_unsupported("%s", 0x%x);' % (e, self.cur_addr)
                ops.append((idx, code))
            body.append((self.cur_addr, ins, ops))
        out = ["void %s(cpu_t *c)" % self.cname(), "{"]
        for (o, z) in sorted(self.ints):
            out.append("    uint64_t u%d_%d = 0;" % (o, z))
        for (o, z) in sorted(self.wides):
            out.append("    rt_wide w%d_%d;" % (o, z))
            out.append("    memset(&w%d_%d, 0, sizeof w%d_%d);" % (o, z, o, z))
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


def is_hex_addr(text):
    try:
        int(text, 16)
        return True
    except ValueError:
        return False


def reachable_from(entry, graph):
    seen = {entry}
    todo = [entry]
    while todo:
        f = todo.pop()
        for g in graph.get(f, ()):
            if g not in seen:
                seen.add(g)
                todo.append(g)
    return seen


def write_report(path, entries, stats, nins, nops, reach=None):
    by_mnem = Counter(m for _, _, m in stats["unsupported_sites"])
    lines = []
    lines.append("# Rapport de traduction (ct3rc.py)")
    lines.append("")
    lines.append("- fonctions traduites : %d" % len(entries))
    lines.append("- instructions : %d" % nins)
    lines.append("- opérations p-code : %d" % nops)
    lines.append("- sites non pris en charge : %d" % len(stats["unsupported_sites"]))
    lines.append("")
    lines.append("## Opérations non prises en charge (par opération p-code)")
    lines.append("")
    for k, v in stats["unsupported"].most_common():
        lines.append("- %s : %d" % (k, v))
    lines.append("")
    lines.append("## Mnémoniques x86 concernés")
    lines.append("")
    for k, v in by_mnem.most_common(60):
        lines.append("- %s : %d" % (k, v))
    lines.append("")
    lines.append("## Fonctions contenant au moins un site non pris en charge")
    lines.append("")
    lines.append("- %d fonctions sur %d" % (len(stats["unsupported_fn_entries"]), len(entries)))
    if reach is not None:
        lines.append("")
        lines.append("## Couverture depuis le point d'entrée (graphe d'appels statique)")
        lines.append("")
        lines.append("- fonctions atteignables par appels directs : %d" % reach["count"])
        lines.append("- dont avec au moins un site non pris en charge : %d" % reach["unsupported"])
        lines.append("- sites d'appels/sauts indirects dans ces fonctions : %d (non résolus)" % reach["indirect"])
        lines.append("- cibles d'appels absentes de la traduction : %d" % reach["missing"])
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pcode_json")
    ap.add_argument("outdir")
    ap.add_argument("--chunk", type=int, default=300, help="fonctions par fichier C")
    ap.add_argument("--entry", default=None, help="adresse du point d'entrée (hex)")
    args = ap.parse_args()

    with open(args.pcode_json) as f:
        functions = json.load(f)

    os.makedirs(args.outdir, exist_ok=True)
    stats = {"unsupported": Counter(), "unsupported_sites": [], "unsupported_fn_entries": set()}
    entries = []
    nins = nops = 0
    translated = []
    graph = {}
    indirect = {}
    for fn in functions:
        nins += len(fn["insns"])
        nops += sum(len(i["pcode"]) for i in fn["insns"])
        t = FunctionTranslator(fn, stats)
        translated.append(t.translate())
        entries.append((t.entry, t.cname()))
        graph[t.entry] = t.callees
        indirect[t.entry] = t.indirect

    reach = None
    if args.entry:
        known = set(graph)
        r = reachable_from(int(args.entry, 16), graph) & known
        bad = r & stats["unsupported_fn_entries"]
        reach = {
            "count": len(r),
            "unsupported": len(bad),
            "indirect": sum(indirect.get(e, 0) for e in r),
            "missing": len({g for e in r for g in graph[e]
                            if g not in known and not (0x7F000000 <= g < 0x7F100000)}),
        }

    chunk_files = []
    for start in range(0, len(translated), args.chunk):
        name = "gen_funcs_%03d.c" % (start // args.chunk)
        chunk_files.append(name)
        body = ["/* Généré par tools/recomp/ct3rc.py : ne pas modifier. */", '#include "rt_fp.h"', ""]
        body.extend(translated[start:start + args.chunk])
        with open(os.path.join(args.outdir, name), "w") as f:
            f.write("\n".join(body))

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
    with open(os.path.join(args.outdir, "chunks.txt"), "w") as f:
        f.write("\n".join(chunk_files) + "\n")

    write_report(os.path.join(args.outdir, "rapport.md"), entries, stats, nins, nops, reach)
    print("fonctions traduites : %d (%d fichiers C)" % (len(entries), len(chunk_files)))
    print("instructions : %d, opérations p-code : %d" % (nins, nops))
    print("sites non pris en charge : %d" % len(stats["unsupported_sites"]))
    for k, v in stats["unsupported"].most_common(12):
        print("  %-20s %d" % (k, v))
    return 0


if __name__ == "__main__":
    sys.exit(main())
