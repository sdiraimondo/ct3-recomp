#!/usr/bin/env python3
"""Pack de données CT3 (lot 1) : extrait l'image de données de CT3.exe.

Le runtime n'a pas besoin du code x86 de CT3.exe : toutes les fonctions sont
traduites en C par tools/recomp/ct3rc.py. Il a besoin de l'image mémoire
(.rdata, .data, en-têtes PE, éventuellement .rsrc) et de la table d'imports.

Ce script produit, dans un dossier de sortie :

  ct3_data.bin   charge utile binaire (sections présentes, alignées)
  ct3_image.h    métadonnées C (constantes, sections, imports)
  ct3_image.json manifeste lisible et vérifiable

Il ne modifie jamais CT3.exe et ne copie aucun asset du jeu.

Exemples :

  python3 tools/pepack/pepack.py "/chemin/CT3.exe" build/pack
  python3 tools/pepack/pepack.py "$CT3_EXE" build/pack \
      --scan-generated build/g3/gen --strict --self-test
  python3 tools/pepack/pepack.py "$CT3_EXE" build/pack \
      --expect-sha256 <empreinte> --dry-run

Sécurité : le pack est un dérivé du binaire du jeu. Il ne doit pas être
versionné ni distribué. Voir README.md et DECISIONS.md (D-016).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from pathlib import Path

PACK_FORMAT = "ct3-pack"
PACK_VERSION = 1
IMAGE_BASE_EXPECTED = 0x00400000
IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_SCN_MEM_WRITE = 0x80000000
IMAGE_SCN_MEM_READ = 0x40000000
PROT_READ = 1
PROT_WRITE = 2
PROT_EXEC = 4

# Motifs d'adresses statiques dans le C généré par ct3rc.py.
ADDRESS_PATTERNS = (
    re.compile(r"guest_ld\s*\(\s*c\s*,\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"guest_st\s*\(\s*c\s*,\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"rt_mem_base\s*\+\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"guest_write\s*\(\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"guest_fill\s*\(\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"guest_note\s*\(\s*0x([0-9A-Fa-f]+)"),
    re.compile(r"guest_st_block\s*\(\s*0x([0-9A-Fa-f]+)"),
)


class PackError(Exception):
    """Erreur de format ou de cohérence du PE / du pack."""


def u16(data: bytes, off: int) -> int:
    if off < 0 or off + 2 > len(data):
        raise PackError(f"lecture 16 bits hors du fichier à 0x{off:x}")
    return struct.unpack_from("<H", data, off)[0]


def u32(data: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(data):
        raise PackError(f"lecture 32 bits hors du fichier à 0x{off:x}")
    return struct.unpack_from("<I", data, off)[0]


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def c_string(value: str) -> str:
    """Échappe une chaîne ASCII pour un littéral C."""
    out = ['"']
    for b in value.encode("utf-8", "replace"):
        ch = chr(b)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif 0x20 <= b < 0x7F:
            out.append(ch)
        else:
            # Évite qu'un \xNN soit suivi d'un chiffre hexadécimal.
            out.append(f"\\x{b:02x}\" \"")
    out.append('"')
    return "".join(out)


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


class Section:
    def __init__(
        self,
        name: str,
        rva: int,
        image_base: int,
        vsize: int,
        raw_size: int,
        raw_ptr: int,
        characteristics: int,
        present: bool,
        reason: str = "",
    ) -> None:
        self.name = name
        self.rva = rva
        self.va = image_base + rva
        self.vsize = vsize
        self.raw_size = raw_size
        self.raw_ptr = raw_ptr
        self.characteristics = characteristics
        self.present = present
        self.reason = reason
        self.offset = 0
        self.copy_size = 0
        self.prot = PROT_READ
        if characteristics & IMAGE_SCN_MEM_WRITE:
            self.prot |= PROT_WRITE
        if characteristics & IMAGE_SCN_MEM_EXECUTE:
            self.prot |= PROT_EXEC

    def contains(self, address: int) -> bool:
        return self.va <= address < self.va + self.vsize


class PEImage:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        self.file_size = len(self.data)
        self.sections: list[Section] = []
        self.imports: list[dict] = []
        self.image_base = 0
        self.image_size = 0
        self.size_of_headers = 0
        self.entry_rva = 0
        self.entry = 0
        self.import_rva = 0
        self.import_size = 0
        self.resource_rva = 0
        self.resource_size = 0
        self._parse()

    def _parse(self) -> None:
        if self.file_size < 0x40 or self.data[:2] != b"MZ":
            raise PackError("en-tête MZ absent")
        pe = u32(self.data, 0x3C)
        if pe + 24 > self.file_size or self.data[pe : pe + 4] != b"PE\0\0":
            raise PackError("signature PE absente")
        machine = u16(self.data, pe + 4)
        nsec = u16(self.data, pe + 6)
        optsz = u16(self.data, pe + 20)
        opt = pe + 24
        if machine != 0x14C:
            raise PackError(f"machine 0x{machine:04x} : PE32 i386 attendu")
        if u16(self.data, opt) != 0x10B:
            raise PackError("optional header PE32 (0x10B) attendu")
        if nsec > 96:
            raise PackError(f"trop de sections ({nsec})")

        self.image_base = u32(self.data, opt + 28)
        self.entry_rva = u32(self.data, opt + 16)
        self.entry = self.image_base + self.entry_rva
        self.image_size = u32(self.data, opt + 56)
        self.size_of_headers = u32(self.data, opt + 60)
        if self.image_base != IMAGE_BASE_EXPECTED:
            raise PackError(
                f"base d'image 0x{self.image_base:08x}, attendu 0x{IMAGE_BASE_EXPECTED:08x}"
            )
        if self.image_size == 0 or self.entry_rva >= self.image_size:
            raise PackError("taille d'image ou point d'entrée invalide")
        if self.size_of_headers > self.file_size:
            raise PackError("SizeOfHeaders dépasse le fichier")

        # Répertoires de données : import (1), resource (2).
        dd = opt + 96
        self.import_rva = u32(self.data, dd + 1 * 8)
        self.import_size = u32(self.data, dd + 1 * 8 + 4)
        self.resource_rva = u32(self.data, dd + 2 * 8)
        self.resource_size = u32(self.data, dd + 2 * 8 + 4)

        sh = opt + optsz
        for i in range(nsec):
            off = sh + 40 * i
            name = self.data[off : off + 8].split(b"\0", 1)[0].decode("latin-1", "replace")
            vsize = u32(self.data, off + 8)
            va = u32(self.data, off + 12)
            raw_size = u32(self.data, off + 16)
            raw_ptr = u32(self.data, off + 20)
            chars = u32(self.data, off + 36)
            if va + max(vsize, raw_size) > self.image_size:
                raise PackError(f"section {name} hors de l'image")
            if raw_size and raw_ptr + raw_size > self.file_size:
                raise PackError(f"section {name} tronquée dans le fichier")
            self.sections.append(
                Section(name, va, self.image_base, vsize, raw_size, raw_ptr, chars, True)
            )

        self._parse_imports()

    def rva_to_offset(self, rva: int) -> int:
        if rva < self.size_of_headers:
            return rva
        for sec in self.sections:
            span = max(sec.vsize, sec.raw_size)
            if sec.rva <= rva < sec.rva + span:
                delta = rva - sec.rva
                if delta >= sec.raw_size:
                    raise PackError(f"RVA 0x{rva:x} dans une zone non initialisée de {sec.name}")
                return sec.raw_ptr + delta
        raise PackError(f"RVA 0x{rva:x} hors des sections")

    def read_cstr(self, rva: int) -> str:
        off = self.rva_to_offset(rva)
        end = self.data.find(b"\0", off)
        if end < 0:
            raise PackError(f"chaîne non terminée à RVA 0x{rva:x}")
        return self.data[off:end].decode("latin-1", "replace")

    def _parse_imports(self) -> None:
        if not self.import_rva:
            return
        off = self.rva_to_offset(self.import_rva)
        index = 0
        while True:
            desc = off + 20 * index
            if desc + 20 > self.file_size:
                raise PackError("table d'imports tronquée")
            original_first_thunk = u32(self.data, desc)
            name_rva = u32(self.data, desc + 12)
            first_thunk = u32(self.data, desc + 16)
            if not any((original_first_thunk, name_rva, first_thunk)):
                break
            if not name_rva or not first_thunk:
                raise PackError(f"descripteur d'import {index} incomplet")
            dll = self.read_cstr(name_rva)
            thunk_rva = original_first_thunk or first_thunk
            thunk_off = self.rva_to_offset(thunk_rva)
            k = 0
            while True:
                entry = u32(self.data, thunk_off + 4 * k)
                if entry == 0:
                    break
                iat_rva = first_thunk + 4 * k
                if entry & 0x80000000:
                    self.imports.append(
                        {"dll": dll, "name": None, "ordinal": entry & 0xFFFF, "iat_rva": iat_rva}
                    )
                else:
                    self.imports.append(
                        {
                            "dll": dll,
                            "name": self.read_cstr(entry + 2),
                            "ordinal": None,
                            "iat_rva": iat_rva,
                        }
                    )
                k += 1
            index += 1

    def select_sections(
        self, include_text: bool, include_rsrc: bool
    ) -> tuple[list[Section], list[Section]]:
        """Retourne (sections présentes, sections exclues)."""
        present: list[Section] = []
        excluded: list[Section] = []

        headers = Section(
            "headers",
            0,
            self.image_base,
            self.size_of_headers,
            self.size_of_headers,
            0,
            IMAGE_SCN_MEM_READ,
            True,
        )
        headers.copy_size = self.size_of_headers
        present.append(headers)

        for sec in self.sections:
            executable = bool(sec.characteristics & IMAGE_SCN_MEM_EXECUTE)
            if sec.name == ".text" and not include_text:
                sec.present = False
                sec.reason = "section exécutable exclue (--include-text pour l'inclure)"
                excluded.append(sec)
                continue
            if executable and not include_text:
                sec.present = False
                sec.reason = "section exécutable exclue (--include-text pour l'inclure)"
                excluded.append(sec)
                continue
            if sec.name == ".rsrc" and not include_rsrc:
                sec.present = False
                sec.reason = "ressources exclues (--no-rsrc)"
                excluded.append(sec)
                continue
            sec.present = True
            sec.copy_size = min(sec.vsize, sec.raw_size) if sec.vsize else sec.raw_size
            if sec.copy_size > sec.raw_size:
                sec.copy_size = sec.raw_size
            present.append(sec)
        return present, excluded

    def build_payload(self, present: list[Section]) -> bytes:
        payload = bytearray()
        for sec in present:
            offset = align_up(len(payload), 16)
            if offset > len(payload):
                payload.extend(b"\0" * (offset - len(payload)))
            sec.offset = offset
            if sec.copy_size:
                payload.extend(self.data[sec.raw_ptr : sec.raw_ptr + sec.copy_size])
            # Aligne la section suivante ; le loader ne lit que copy_size octets.
            end = align_up(len(payload), 16)
            if end > len(payload):
                payload.extend(b"\0" * (end - len(payload)))
        return bytes(payload)

    def section_for(self, address: int) -> Section | None:
        for sec in self.sections:
            if sec.contains(address):
                return sec
        return None

    def check_addresses(self, addresses: list[int]) -> dict:
        packed = 0
        excluded = 0
        gap = 0
        outside = 0
        excluded_examples: list[int] = []
        gap_examples: list[int] = []
        outside_examples: list[int] = []
        for address in addresses:
            sec = self.section_for(address)
            if sec is not None:
                if sec.present:
                    packed += 1
                else:
                    excluded += 1
                    if len(excluded_examples) < 10:
                        excluded_examples.append(address)
                continue
            if self.image_base <= address < self.image_base + self.image_size:
                gap += 1
                if len(gap_examples) < 10:
                    gap_examples.append(address)
            else:
                outside += 1
                if len(outside_examples) < 10:
                    outside_examples.append(address)
        return {
            "packed": packed,
            "excluded": excluded,
            "gap": gap,
            "outside": outside,
            "excluded_examples": excluded_examples,
            "gap_examples": gap_examples,
            "outside_examples": outside_examples,
        }


def scan_generated(path: Path) -> tuple[set[int], int]:
    addresses: set[int] = set()
    references = 0
    files = 0
    for file in sorted(path.rglob("*.c")):
        files += 1
        text = file.read_text(errors="replace")
        for pattern in ADDRESS_PATTERNS:
            for match in pattern.finditer(text):
                references += 1
                addresses.add(int(match.group(1), 16))
    if files == 0:
        raise PackError(f"aucun fichier .c dans {path}")
    return addresses, references


def read_address_file(path: Path) -> list[int]:
    addresses: list[int] = []
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        line = line.split("#", 1)[0].strip()
        try:
            addresses.append(int(line, 16))
        except ValueError as exc:
            raise PackError(f"adresse invalide dans {path}: {line!r}") from exc
    return addresses


def header_text(
    pe: PEImage,
    present: list[Section],
    excluded: list[Section],
    payload: bytes,
    source_sha: str,
) -> str:
    payload_sha = sha256_hex(payload)
    has_text = any(sec.name == ".text" and sec.present for sec in pe.sections)
    lines: list[str] = []
    lines.append("/* Généré par tools/pepack/pepack.py. Ne pas modifier. */")
    lines.append("/* Pack de données CT3 : dérivé de votre copie de CT3.exe, à ne pas versionner. */")
    lines.append("#pragma once")
    lines.append("#include <stddef.h>")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#ifdef __cplusplus")
    lines.append('extern "C" {')
    lines.append("#endif")
    lines.append("")
    lines.append(f"#define CT3_PACK_FORMAT {c_string(PACK_FORMAT)}")
    lines.append(f"#define CT3_PACK_VERSION {PACK_VERSION}u")
    lines.append(f"#define CT3_IMAGE_BASE 0x{pe.image_base:08x}u")
    lines.append(f"#define CT3_IMAGE_SIZE 0x{pe.image_size:08x}u")
    lines.append(f"#define CT3_IMAGE_ENTRY 0x{pe.entry:08x}u")
    lines.append(f"#define CT3_IMAGE_DATA_SIZE 0x{len(payload):08x}u")
    lines.append(f"#define CT3_IMAGE_HAS_TEXT {1 if has_text else 0}u")
    lines.append(f"#define CT3_IMAGE_SOURCE_SHA256 {c_string(source_sha)}")
    lines.append(f"#define CT3_IMAGE_PAYLOAD_SHA256 {c_string(payload_sha)}")
    lines.append("")
    lines.append("typedef struct ct3_pack_section {")
    lines.append("    const char *name;   /* nom PE, ou \"headers\" */")
    lines.append("    uint32_t va;        /* adresse invitée */")
    lines.append("    uint32_t vsize;     /* taille virtuelle, BSS comprise */")
    lines.append("    uint32_t offset;    /* offset dans ct3_data.bin, 0 si absent */")
    lines.append("    uint32_t raw_size;  /* octets présents dans le pack, 0 si absent */")
    lines.append("    uint32_t prot;      /* 1=R, 2=W, 4=X */")
    lines.append("    uint32_t present;   /* 1 = données dans le pack, 0 = à zéro-remplir */")
    lines.append("} ct3_pack_section;")
    lines.append("")
    lines.append("typedef struct ct3_pack_import {")
    lines.append("    uint32_t iat_rva;   /* RVA de l'entrée IAT (base 0x400000) */")
    lines.append("    const char *dll;")
    lines.append("    const char *name;   /* NULL si import par ordinal */")
    lines.append("    uint32_t ordinal;   /* 0 si import par nom */")
    lines.append("} ct3_pack_import;")
    lines.append("")
    lines.append("static const ct3_pack_section ct3_pack_sections[] = {")
    for sec in present + excluded:
        lines.append(
            "    {%s, 0x%08xu, 0x%08xu, 0x%08xu, 0x%08xu, 0x%xu, 0x%xu},"
            % (
                c_string(sec.name),
                sec.va,
                sec.vsize,
                sec.offset if sec.present else 0,
                sec.copy_size if sec.present else 0,
                sec.prot,
                1 if sec.present else 0,
            )
        )
    lines.append("};")
    lines.append(
        "static const unsigned ct3_pack_section_count = "
        "sizeof(ct3_pack_sections) / sizeof(ct3_pack_sections[0]);"
    )
    lines.append("")
    lines.append("static const ct3_pack_import ct3_pack_imports[] = {")
    for imp in pe.imports:
        name = "NULL" if imp["name"] is None else c_string(imp["name"])
        ordinal = 0 if imp["ordinal"] is None else imp["ordinal"]
        lines.append(
            "    {0x%08xu, %s, %s, 0x%xu},"
            % (imp["iat_rva"], c_string(imp["dll"]), name, ordinal)
        )
    if not pe.imports:
        lines.append("    {0, NULL, NULL, 0},")
    lines.append("};")
    lines.append(
        "static const unsigned ct3_pack_import_count = "
        "sizeof(ct3_pack_imports) / sizeof(ct3_pack_imports[0]);"
    )
    lines.append("")
    lines.append("#ifdef __cplusplus")
    lines.append("}")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def json_text(
    pe: PEImage,
    present: list[Section],
    excluded: list[Section],
    payload: bytes,
    source_sha: str,
) -> str:
    def hex32(value: int) -> str:
        return f"0x{value:08x}"

    def section_json(sec: Section) -> dict:
        return {
            "name": sec.name,
            "va": hex32(sec.va),
            "vsize": hex32(sec.vsize),
            "offset": hex32(sec.offset if sec.present else 0),
            "raw_size": hex32(sec.copy_size if sec.present else 0),
            "prot": sec.prot,
            "present": bool(sec.present),
            "reason": sec.reason,
        }

    dlls = sorted({imp["dll"] for imp in pe.imports})
    manifest = {
        "format": PACK_FORMAT,
        "version": PACK_VERSION,
        "source": {
            "name": pe.path.name,
            "size": pe.file_size,
            "sha256": source_sha,
        },
        "image": {
            "base": hex32(pe.image_base),
            "size": hex32(pe.image_size),
            "entry": hex32(pe.entry),
            "has_text": any(sec.name == ".text" and sec.present for sec in pe.sections),
            "data_size": hex32(len(payload)),
            "payload_sha256": sha256_hex(payload),
        },
        "sections": [section_json(sec) for sec in present + excluded],
        "imports": [
            {
                "dll": imp["dll"],
                "name": imp["name"],
                "ordinal": imp["ordinal"],
                "iat_rva": hex32(imp["iat_rva"]),
            }
            for imp in pe.imports
        ],
        "stats": {
            "sections_total": len(present) + len(excluded),
            "sections_present": len(present),
            "sections_excluded": len(excluded),
            "imports": len(pe.imports),
            "dlls": len(dlls),
        },
    }
    return json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"


def self_test(outdir: Path, pe: PEImage, present: list[Section], payload: bytes) -> None:
    data_path = outdir / "ct3_data.bin"
    if not data_path.is_file():
        raise PackError(f"{data_path} absent")
    data = data_path.read_bytes()
    if data != payload:
        raise PackError("ct3_data.bin ne correspond pas à la charge utile générée")
    if sha256_hex(data) != sha256_hex(payload):
        raise PackError("empreinte de la charge utile incohérente")
    for sec in present:
        if sec.offset + sec.copy_size > len(data):
            raise PackError(f"section {sec.name} hors de ct3_data.bin")
        if sec.offset % 16:
            raise PackError(f"section {sec.name} non alignée")
    for imp in pe.imports:
        address = pe.image_base + imp["iat_rva"]
        if not any(sec.present and sec.contains(address) for sec in pe.sections):
            raise PackError(f"IAT 0x{imp['iat_rva']:x} hors des sections présentes")
    if pe.entry_rva >= pe.image_size:
        raise PackError("point d'entrée hors image")
    print("[self-test] pack cohérent")


def print_summary(pe: PEImage, present: list[Section], excluded: list[Section], payload: bytes) -> None:
    print(f"[pack] source : {pe.path.name} ({pe.file_size} octets, sha256 {sha256_hex(pe.data)})")
    print(
        f"[pack] image  : base=0x{pe.image_base:08x} taille=0x{pe.image_size:08x} "
        f"entrée=0x{pe.entry:08x}"
    )
    print(f"[pack] charge utile : {len(payload)} octets (0x{len(payload):x})")
    print("[pack] sections présentes :")
    for sec in present:
        print(
            f"  {sec.name:8s} va=0x{sec.va:08x} vsize=0x{sec.vsize:08x} "
            f"offset=0x{sec.offset:08x} raw=0x{sec.copy_size:08x} prot={sec.prot}"
        )
    if excluded:
        print("[pack] sections exclues :")
        for sec in excluded:
            print(f"  {sec.name:8s} va=0x{sec.va:08x} vsize=0x{sec.vsize:08x} ({sec.reason})")
    dlls = sorted({imp["dll"] for imp in pe.imports})
    print(f"[pack] imports : {len(pe.imports)} dans {len(dlls)} DLL")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Génère le pack de données CT3 à partir de CT3.exe (lot 1)."
    )
    parser.add_argument("exe", type=Path, help="chemin de votre copie de CT3.exe (lecture seule)")
    parser.add_argument("outdir", type=Path, help="dossier de sortie (build/pack)")
    parser.add_argument("--include-text", action="store_true", help="inclure .text (débogage)")
    parser.add_argument(
        "--no-rsrc", action="store_true", help="exclure .rsrc (le runtime actuel ne l'utilise pas)"
    )
    parser.add_argument(
        "--check-addresses",
        type=Path,
        metavar="FICHIER",
        help="vérifier une liste d'adresses hexadécimales (une par ligne)",
    )
    parser.add_argument(
        "--scan-generated",
        type=Path,
        metavar="DOSSIER",
        help="vérifier les adresses statiques du C généré (build/g3/gen)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="échouer si une adresse tombe dans une section exclue ou un trou",
    )
    parser.add_argument(
        "--strict-outside",
        action="store_true",
        help="échouer aussi pour les adresses hors image (code mort, MMIO)",
    )
    parser.add_argument("--self-test", action="store_true", help="vérifier les fichiers écrits")
    parser.add_argument("--dry-run", action="store_true", help="analyser sans écrire")
    parser.add_argument("--expect-sha256", metavar="HEX", help="refuser un autre CT3.exe")
    parser.add_argument("--quiet", action="store_true", help="sortie minimale")
    args = parser.parse_args(argv)

    try:
        pe = PEImage(args.exe)
        source_sha = sha256_hex(pe.data)
        if args.expect_sha256 and source_sha.lower() != args.expect_sha256.lower():
            raise PackError(
                f"empreinte de CT3.exe inattendue : {source_sha} "
                f"(attendu {args.expect_sha256})"
            )

        present, excluded = pe.select_sections(
            include_text=args.include_text, include_rsrc=not args.no_rsrc
        )
        payload = pe.build_payload(present)

        if not args.quiet:
            print_summary(pe, present, excluded, payload)

        addresses: set[int] = set()
        references = 0
        if args.scan_generated:
            scanned, references = scan_generated(args.scan_generated)
            addresses |= scanned
        if args.check_addresses:
            addresses |= set(read_address_file(args.check_addresses))

        if addresses:
            check = pe.check_addresses(sorted(addresses))
            print(
                f"[check] {len(addresses)} adresses uniques, {references} références : "
                f"packed={check['packed']} excluded={check['excluded']} "
                f"gap={check['gap']} outside={check['outside']}"
            )
            if check["excluded_examples"]:
                print(
                    "[check] adresses dans une section exclue : "
                    + ", ".join(f"0x{a:08x}" for a in check["excluded_examples"])
                )
            if check["gap_examples"]:
                print(
                    "[check] adresses dans un trou de l'image : "
                    + ", ".join(f"0x{a:08x}" for a in check["gap_examples"])
                )
            if check["outside_examples"]:
                print(
                    "[check] adresses hors image (ignorées par défaut) : "
                    + ", ".join(f"0x{a:08x}" for a in check["outside_examples"])
                )
            failed = False
            if args.strict and (check["excluded"] or check["gap"]):
                failed = True
            if args.strict_outside and check["outside"]:
                failed = True
            if failed:
                raise PackError("vérification stricte échouée")
        elif args.strict:
            raise PackError("--strict demande --check-addresses ou --scan-generated")

        if args.dry_run:
            print("[pack] dry-run : aucun fichier écrit")
            return 0

        args.outdir.mkdir(parents=True, exist_ok=True)
        data_path = args.outdir / "ct3_data.bin"
        header_path = args.outdir / "ct3_image.h"
        json_path = args.outdir / "ct3_image.json"
        data_path.write_bytes(payload)
        header_path.write_text(header_text(pe, present, excluded, payload, source_sha))
        json_path.write_text(json_text(pe, present, excluded, payload, source_sha))
        if not args.quiet:
            print(f"[pack] écrit : {data_path}")
            print(f"[pack] écrit : {header_path}")
            print(f"[pack] écrit : {json_path}")

        if args.self_test:
            self_test(args.outdir, pe, present, payload)
        return 0
    except (OSError, PackError) as exc:
        print(f"pepack: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
