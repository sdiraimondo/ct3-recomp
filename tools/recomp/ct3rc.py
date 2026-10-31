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


# Userops MMX/3DNow!/SSE implémentés dans runtime/src/rt_vec.c : index -> (nom, arité).
VEC_USEROPS = {
    136: ("PACKSSWB", 2),
    137: ("PACKSSDW", 2),
    142: ("PADDSW", 2),
    169: ("PSUBSW", 2),
    154: ("PMADDWD", 2),
    157: ("PMULHW", 2),
    166: ("PSLLW", 2),
    167: ("PSRAW", 2),
    238: ("PFADD", 2),
    239: ("PFSUB", 2),
    240: ("PFSUBR", 2),
    243: ("PFMUL", 2),
    241: ("PFMIN", 2),
    242: ("PFMAX", 2),
    234: ("PFCMPGE", 2),
    235: ("PFCMPGT", 2),
    236: ("PFCMPEQ", 2),
    237: ("PFACC", 2),
    254: ("PFNACC", 2),
    256: ("PSWAPD", 2),
    232: ("PI2FD", 2),
    233: ("PF2ID", 2),
    244: ("PFRCP", 2),
    245: ("PFRSQRT", 2),
    246: ("PFRCPIT1", 2),
    247: ("PFRSQIT1", 2),
    248: ("PFRCPIT2", 2),
    173: ("RCPPS", 2),
    174: ("RCPSS", 2),
    176: ("RSQRTSS", 2),
}
FEMMS_USEROP = 251


def mask(size):
    return MASK_BITS[size]


TERMINAL_MNEMONICS = ("RET", "RETN", "JMP", "HLT", "INT3", "UD2")

# CT3_TRACE_INSN=1 : chaque instruction met à jour rt_cur_insn (diagnostic de plantage, build de débogage).
TRACE_INSN = os.environ.get("CT3_TRACE_INSN") == "1"

# __setjmp3 du CRT MSVC (CT3.exe) : l'appel est remplacé par un setjmp hôte dans le cadre appelant.
SETJMP_ENTRY = 0x0051A238


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
        self.jmpbufs = set()
        self.cur_next = None
        self.indirect = 0
        self.cur_addr = 0
        self.cur_targets = []
        self.cur_is_last = False
        self.cur_op_index = 0

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

    @staticmethod
    def is_wide_int_op(m, out, ins):
        if m not in ("INT_AND", "INT_OR", "INT_XOR", "INT_NEGATE", "INT_RIGHT", "INT_LEFT", "INT_ZEXT"):
            return False
        return any(v is not None and v["z"] > 8 for v in ([out] if out else []) + ins)

    def emit_wide_int(self, m, out, ins):
        n = out["z"]
        if m in ("INT_AND", "INT_OR", "INT_XOR"):
            fn = {"INT_AND": "rt_w_and", "INT_OR": "rt_w_or", "INT_XOR": "rt_w_xor"}[m]
            return "%s(%s, %s, %s, %d);" % (fn, self.ptr(out), self.ptr(ins[0]), self.ptr(ins[1]), n)
        if m == "INT_NEGATE":
            return "rt_w_not(%s, %s, %d);" % (self.ptr(out), self.ptr(ins[0]), n)
        if m in ("INT_RIGHT", "INT_LEFT"):
            if ins[1]["s"] != "const":
                raise Unsupported("décalage de valeur de %d octets par variable" % n)
            fn = "rt_w_shr" if m == "INT_RIGHT" else "rt_w_shl"
            return "%s(%s, %s, %d, %d);" % (fn, self.ptr(out), self.ptr(ins[0]), n, ins[1]["o"])
        if m == "INT_ZEXT":
            return "rt_w_zext(%s, %s, %d, %d);" % (self.ptr(out), self.ptr(ins[0]), ins[0]["z"], n)
        raise Unsupported(m)

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

        if self.is_wide_int_op(m, out, ins):
            return self.emit_wide_int(m, out, ins)
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
            if tgt == SETJMP_ENTRY and self.cur_next in self.insn_addrs:
                # setjmp hôte dans ce cadre : un longjmp invité reprend ici (voir rt_jmp.c).
                r = self.cur_next
                self.jmpbufs.add(r)
                self.ram_labels.add(r)
                return ("if (setjmp(jb_%08x) != 0) goto A_%08x; rt_cs_push(0x%x); dispatch(c, 0x%x); rt_cs_pop(); "
                        "rt_jmp_register(c, &jb_%08x, (uint32_t)guest_ld(c, (uint32_t)cpu_rd(c, 16, 4), 4), 0x%x);"
                        % (r, r, r, tgt, r, r))
            r = self.cur_next
            if TRACE_INSN:
                return ("{ uint32_t s_[4]; rt_regs_save(c, s_); rt_cs_push(0x%x); dispatch(c, 0x%x); rt_cs_pop(); "
                        "rt_regs_check(c, s_, 0x%x, 0x%x); }" % (r, tgt, self.cur_addr, tgt))
            return "rt_cs_push(0x%x); dispatch(c, 0x%x); rt_cs_pop();" % (r, tgt)
        if m == "CALLIND":
            self.indirect += 1
            r = self.cur_next
            if TRACE_INSN:
                return ("{ uint32_t s_[4]; rt_regs_save(c, s_); rt_cs_push(0x%x); dispatch(c, (uint32_t)%s); "
                        "rt_cs_pop(); rt_regs_check(c, s_, 0x%x, 0); }" % (r, a(0), self.cur_addr))
            return "rt_cs_push(0x%x); dispatch(c, (uint32_t)%s); rt_cs_pop();" % (r, a(0))
        if m == "RETURN":
            if not ins:
                return "return;"
            # Un RET peut servir de saut (prologues SEH du CRT : push cible ; ret). Un retour normal
            # correspond au sommet de la pile des adresses de retour (empilée à chaque appel).
            return ("if (rt_cs_top() != (uint32_t)%s) dispatch(c, (uint32_t)%s); return;" % (a(0), a(0)))
        if m == "BRANCHIND":
            self.indirect += 1
            targets = self.cur_targets
            if not targets:
                # Saut indirect sans cible Ghidra : tail-call vers une fonction (table de pointeurs).
                # Si la cible n'est pas une fonction traduite, dispatch s'arrête avec un message explicite.
                return "dispatch(c, (uint32_t)%s); return;" % a(0)
            parts = ["{ uint32_t t_ = (uint32_t)%s;" % a(0)]
            for t in targets:
                if t in self.insn_addrs:
                    self.ram_labels.add(t)
                    parts.append("if (t_ == 0x%x) goto A_%08x;" % (t, t))
                else:
                    self.callees.add(t)
                    parts.append("if (t_ == 0x%x) { dispatch(c, t_); return; }" % t)
            # Valeur absente de la liste Ghidra (ex. jmp [IAT] : cible d'import) : répartition à l'exécution.
            parts.append("dispatch(c, t_); return; }")
            return " ".join(parts)
        if m == "BRANCH":
            tgt = ins[0]
            if tgt["s"] == "const":
                k = self.local_target(tgt)
                self.op_labels.add((self.cur_addr, k))
                return "goto P_%08x_%d;" % (self.cur_addr, k)
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
                k = self.local_target(tgt)
                self.op_labels.add((self.cur_addr, k))
                return "if (%s) goto P_%08x_%d;" % (cond, self.cur_addr, k)
            t = tgt["o"]
            if t in self.insn_addrs:
                self.ram_labels.add(t)
                return "if (%s) goto A_%08x;" % (cond, t)
            self.callees.add(t)
            return "if (%s) { dispatch(c, 0x%x); return; }" % (cond, t)
        raise Unsupported(m)

    def local_target(self, tgt):
        """Cible d'un saut interne à l'instruction : décalage relatif à l'index de l'opération (entier signé 32 bits)."""
        off = tgt["o"] & 0xFFFFFFFF
        if off >= 0x80000000:
            off -= 0x100000000
        return self.cur_op_index + off

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
            if osz > 8:
                return "rt_fput(%s, %d, truncl(rt_fget(%s, %d)));" % (
                    self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
            return self.int_assign(out, "rt_ftrunc(%s, %d, %d)" % (self.ptr(ins[0]), ins[0]["z"], osz))
        if m == "ROUND":
            return "rt_fput(%s, %d, rt_fround(c, rt_fget(%s, %d)));" % (
                self.ptr(out), osz, self.ptr(ins[0]), ins[0]["z"])
        raise Unsupported(m)

    def emit_callother(self, out, ins):
        idx = ins[0]["o"]
        if idx == 16:
            return "rt_int3(0x%x);" % self.cur_addr
        if idx in (17, 18):
            return "(void)0; /* marqueur XCHG : l'échange est fait par les opérations voisines */"
        if idx == 76:
            return self.int_assign(out, "rt_rdtsc()")
        if 45 <= idx <= 61:
            return self.int_assign(out, "rt_cpuid_block(c)")
        if idx == FEMMS_USEROP:
            return "cpu_wr(c, 4260, 2, 0xFFFF);"
        if idx in VEC_USEROPS:
            name, arity = VEC_USEROPS[idx]
            args = ins[1:]
            if len(args) != arity:
                raise Unsupported("CALLOTHER %s : %d entrées" % (name, len(args)))
            return "rt_v_%s(%s, %s, %s);" % (name, self.ptr(out), self.ptr(args[0]), self.ptr(args[1]))
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
            self.cur_next = int(ins["addr"], 16) + int(ins["len"])
            self.cur_is_last = n == last
            ops = []
            for idx, op in enumerate(ins["pcode"]):
                self.cur_op_index = idx
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
        for r in sorted(self.jmpbufs):
            out.append("    jmp_buf jb_%08x;" % r)
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
                if TRACE_INSN and idx == 0:
                    out.append("    rt_cur_insn = 0x%08x; rt_probe(c, 0x%08x);" % (addr, addr))
                out.append("    " + code + "  /* %s */" % ins["text"].replace("*/", "*\\/"))
        last_ins = self.fn["insns"][-1] if self.fn["insns"] else None
        if last_ins is not None and last_ins["text"].split(" ")[0] not in TERMINAL_MNEMONICS:
            # Dernière instruction non terminale : l'exécution tombe dans le code suivant, qui peut être
            # une autre fonction (Ghidra coupe la fonction à la fin de son corps).
            nxt = int(last_ins["addr"], 16) + int(last_ins["len"])
            out.append("    dispatch(c, 0x%x); return;  /* chute vers l'instruction suivante */" % nxt)
        out.append("    return;")
        out.append("}")
        out.append("")
        return "\n".join(out)


def interior_entries(functions):
    """Points d'entrée atteints par branchement ou appel direct, qui ne sont pas le début d'une fonction
    (ex. continuation d'un gestionnaire SEH). Le code est la suite linéaire des instructions de la
    fonction qui les contient, jusqu'au premier RET (inclus)."""
    entry_set = {int(f["entry"], 16) for f in functions}
    owner = {}
    for f in functions:
        for idx, ins in enumerate(f["insns"]):
            owner[int(ins["addr"], 16)] = (f, idx)
    wanted = set()
    for f in functions:
        own = {int(i["addr"], 16) for i in f["insns"]}
        for ins in f["insns"]:
            for op in ins["pcode"]:
                if op["op"] not in ("BRANCH", "CBRANCH", "CALL") or not op["in"]:
                    continue
                t = op["in"][0]
                if t["s"] != "ram":
                    continue
                target = t["o"]
                if target in own or target in entry_set or target not in owner:
                    continue
                wanted.add(target)
    extra = []
    for target in sorted(wanted):
        f, idx = owner[target]
        insns = []
        for ins in f["insns"][idx:]:
            insns.append(ins)
            if any(p["op"] == "RETURN" for p in ins["pcode"]):
                break
        extra.append({"name": "ENTRY_%08x" % target, "entry": "0x%08x" % target,
                      "insns": insns, "thunk": False})
    return extra


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
    extra = interior_entries(functions)
    functions = functions + extra
    stats = {"unsupported": Counter(), "unsupported_sites": [], "unsupported_fn_entries": set()}
    entries = []
    nins = nops = 0
    translated = []
    graph = {}
    indirect = {}
    print("points d'entrée intérieurs ajoutés : %d" % len(extra))
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
        body = ["/* Généré par tools/recomp/ct3rc.py : ne pas modifier. */",
                '#include <setjmp.h>', '#include "rt_fp.h"', '#include "rt_vec.h"', '#include "rt_jmp.h"', ""]
        body.extend(translated[start:start + args.chunk])
        with open(os.path.join(args.outdir, name), "w") as f:
            f.write("\n".join(body))

    with open(os.path.join(args.outdir, "gen_funcs.h"), "w") as f:
        f.write("/* Généré par tools/recomp/ct3rc.py */\n#pragma once\n#include \"rt.h\"\n")
        for _, cname in entries:
            f.write("void %s(cpu_t *c);\n" % cname)
    with open(os.path.join(args.outdir, "gen_dispatch.c"), "w") as f:
        f.write("/* Généré par tools/recomp/ct3rc.py : ne pas modifier. */\n")
        f.write('#include "rt.h"\n#include "rt_jmp.h"\n#include "gen_funcs.h"\n\n')
        f.write("void dispatch(cpu_t *c, uint32_t addr)\n{\n")
        f.write("    if (addr >= RT_STUB_BASE && addr < RT_STUB_END) {\n")
        f.write("        rt_shim_call(c, addr);\n        return;\n    }\n")
        f.write("    rt_jmp_try(c, addr);\n")
        if TRACE_INSN:
            f.write("    rt_watch_entry(c, addr);\n")
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
