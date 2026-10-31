#!/usr/bin/env python3
"""Génère runtime/src/rt_d3d9_gen.cpp : stubs COM invités -> interfaces D3D9 hôtes (dxvk-native).

Source : d3d9.h de dxvk-native (DECLARE_INTERFACE_ / STDMETHOD). L'héritage est aplati dans l'ordre
des vtables. Chaque méthode déclarante devient une fonction
    uint32_t m_<Déclarant>_<Méthode>(cpu_t *c, const uint32_t *a)
qui convertit les arguments invités (32 bits) vers les types hôtes, appelle la méthode hôte et
convertit le résultat. Les méthodes listées dans CUSTOM sont écrites à la main dans
runtime/src/rt_d3d9_custom.cpp (fonctions custom_<Déclarant>_<Méthode>).

Usage : gen_com.py <dxvk/include/d3d9.h> <sortie.cpp>
"""

import re
import sys

INTERFACES = [
    "IDirect3D9",
    "IDirect3DDevice9",
    "IDirect3DResource9",
    "IDirect3DSwapChain9",
    "IDirect3DSurface9",
    "IDirect3DVolume9",
    "IDirect3DVertexBuffer9",
    "IDirect3DIndexBuffer9",
    "IDirect3DBaseTexture9",
    "IDirect3DTexture9",
    "IDirect3DCubeTexture9",
    "IDirect3DVolumeTexture9",
    "IDirect3DVertexDeclaration9",
    "IDirect3DVertexShader9",
    "IDirect3DPixelShader9",
    "IDirect3DStateBlock9",
    "IDirect3DQuery9",
]

BASES = {
    "IDirect3DSurface9": "IDirect3DResource9",
    "IDirect3DVertexBuffer9": "IDirect3DResource9",
    "IDirect3DIndexBuffer9": "IDirect3DResource9",
    "IDirect3DBaseTexture9": "IDirect3DResource9",
    "IDirect3DTexture9": "IDirect3DBaseTexture9",
    "IDirect3DCubeTexture9": "IDirect3DBaseTexture9",
    "IDirect3DVolumeTexture9": "IDirect3DBaseTexture9",
}

# Méthodes écrites à la main (rt_d3d9_custom.cpp). Clé : "Déclarant::Méthode".
CUSTOM = {
    "IUnknown::QueryInterface",
    "IUnknown::Release",
    "IDirect3D9::GetAdapterMonitor",
    "IDirect3DDevice9::GetCreationParameters",
    "IDirect3DDevice9::CreateVertexBuffer",
    "IDirect3DDevice9::CreateIndexBuffer",
    "IDirect3DDevice9::CreateTexture",
    "IDirect3DDevice9::Present",
    "IDirect3DVertexBuffer9::Lock",
    "IDirect3DVertexBuffer9::Unlock",
    "IDirect3DIndexBuffer9::Lock",
    "IDirect3DIndexBuffer9::Unlock",
    "IDirect3DSurface9::LockRect",
    "IDirect3DSurface9::UnlockRect",
    "IDirect3DTexture9::LockRect",
    "IDirect3DTexture9::UnlockRect",
    "IDirect3DCubeTexture9::LockRect",
    "IDirect3DCubeTexture9::UnlockRect",
    "IDirect3DVolumeTexture9::LockBox",
    "IDirect3DVolumeTexture9::UnlockBox",
    "IDirect3DVolume9::LockBox",
    "IDirect3DVolume9::UnlockBox",
}

IFACE_NAMES = set(INTERFACES) | {"IUnknown"}

# Handles Win32 passés comme scalaires : valeurs invitées, converties sans pointeur.
HANDLE_TYPES = {"HANDLE", "HMONITOR", "HDC", "HICON", "HCURSOR", "HINSTANCE", "HMODULE"}


def read_decls(text):
    """Retourne {nom_interface: (base, corps)} pour les DECLARE_INTERFACE_ du fichier."""
    out = {}
    for m in re.finditer(r"DECLARE_INTERFACE_\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*\{", text):
        name, base = m.group(1), m.group(2)
        depth = 1
        i = m.end()
        while depth:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        out[name] = (base, text[m.end():i - 1])
    return out


METHOD_RE = re.compile(
    r"STDMETHOD(?P<under>_?)\s*\((?P<head>[^)]*)\)\s*\(\s*THIS_?\s*(?P<args>[^)]*)\)\s*PURE\s*;",
    re.S,
)


def parse_methods(body):
    res = []
    for m in METHOD_RE.finditer(body):
        head = m.group("head")
        if m.group("under"):
            ret, name = [x.strip() for x in head.split(",", 1)]
        else:
            ret, name = "HRESULT", head.strip()
        args_text = " ".join(m.group("args").split())
        args = []
        if args_text and args_text != "void":
            for i, a in enumerate(args_text.split(",")):
                args.append(parse_arg(a.strip(), i))
        res.append((ret, name, args))
    return res


def parse_arg(text, index):
    """Retourne (type, nom). Le type garde ses qualifiants (const, pointeurs). Un paramètre sans nom
    (fréquent dans d3d9.h, ex. « CONST D3DLIGHT9* ») reçoit le nom p<index>."""
    text = re.sub(r"\bCONST\b", "const", text.strip())
    text = re.sub(r"\bstruct\s+", "", text)
    if text.endswith("*"):
        return text, "p%d" % index
    m = re.match(r"^(.*[^\w])(\w+)$", text)
    if m is None or m.group(1).strip() in ("", "const"):
        return text, "p%d" % index
    return m.group(1).strip(), m.group(2)


IUNKNOWN_METHODS = ("QueryInterface", "AddRef", "Release")


def flatten(decls, name):
    """Retourne [(déclarant, retour, méthode, arguments)] dans l'ordre de la vtable.

    Dans d3d9.h, le corps de chaque interface dérivée redéclare la vtable complète (méthodes IUnknown,
    puis celles de la base, puis les siennes) : le corps donne donc directement l'ordre des slots.
    Les trois méthodes IUnknown sont attribuées à IUnknown pour partager leurs fonctions."""
    if name == "IUnknown":
        return [
            ("IUnknown", "HRESULT", "QueryInterface", [("REFIID", "riid"), ("void**", "ppvObject")]),
            ("IUnknown", "ULONG", "AddRef", []),
            ("IUnknown", "ULONG", "Release", []),
        ]
    _base, body = decls[name]
    out = []
    for ret, mname, args in parse_methods(body):
        # Les méthodes IUnknown sont en tête du corps, avant toute méthode propre à l'interface.
        is_unknown = mname in IUNKNOWN_METHODS and len(out) < 3
        out.append(("IUnknown" if is_unknown else name, ret, mname, args))
    return out


def classify(typ):
    """Retourne (classe, base) pour un type de paramètre."""
    ptrs = typ.count("*")
    base = typ.replace("*", "").replace("const", "").strip()
    if base in IFACE_NAMES and ptrs == 1:
        return "iface_in", base
    if base in IFACE_NAMES and ptrs == 2:
        return "iface_out", base
    if base == "HWND" and ptrs == 0:
        return "hwnd", base
    if base == "float" and ptrs == 0:
        return "float", base
    if base == "D3DPRESENT_PARAMETERS" and ptrs == 1:
        return "pp", base
    if ptrs >= 1:
        return "ptr", typ
    return "scalar", base


def gen_method(owner, ret, name, args):
    """Retourne (nom_de_fonction, code). Le code est vide pour les méthodes sur mesure."""
    key = "%s::%s" % (owner, name)
    if key in CUSTOM:
        return "custom_%s_%s" % (owner, name), ""
    fname = "m_%s_%s" % (owner, name)
    lines = ["static uint32_t %s(cpu_t *c, const uint32_t *a)" % fname, "{", "    (void)c;"]
    lines.append("    %s *self = rt_self<%s>(a[0]);" % (owner, owner))
    call_args = []
    post = []
    for k, (typ, _pname) in enumerate(args, start=1):
        cls, base = classify(typ)
        if cls == "iface_in":
            call_args.append("(%s *)rt_com_host(a[%d])" % (base, k))
        elif cls == "iface_out":
            lines.append("    %s *o%d = nullptr;" % (base, k))
            call_args.append("&o%d" % k)
            post.append("    rt_com_out(a[%d], o%d, %d);" % (k, k, INTERFACES.index(base)))
        elif cls == "hwnd":
            call_args.append("rt_hwnd(a[%d])" % k)
        elif cls == "float":
            call_args.append("rt_u2f(a[%d])" % k)
        elif cls == "pp":
            lines.append("    D3DPRESENT_PARAMETERS pp%d;" % k)
            lines.append("    if (a[%d]) rt_pp_in(a[%d], pp%d);" % (k, k, k))
            call_args.append("a[%d] ? &pp%d : nullptr" % (k, k))
        elif cls == "ptr":
            call_args.append("(%s)rt_gptr(a[%d])" % (base, k))
        else:
            if base in HANDLE_TYPES:
                call_args.append("(%s)(uintptr_t)a[%d]" % (base, k))
            else:
                call_args.append("(%s)a[%d]" % (base, k))
    call = "self->%s(%s)" % (name, ", ".join(call_args))
    if ret == "void":
        lines.append("    %s;" % call)
        lines += post
        lines.append("    return 0;")
    else:
        lines.append("    %s r = %s;" % (ret, call))
        lines += post
        lines.append("    return (uint32_t)r;")
    lines.append("}")
    return fname, "\n".join(lines)


def main():
    src, dst = sys.argv[1], sys.argv[2]
    decls = read_decls(open(src, encoding="latin-1").read())

    functions = []      # code des fonctions générées
    declared = set()    # fonctions sur mesure à déclarer
    fn_of = {}          # (déclarant, méthode) -> nom de fonction
    tables = []
    for iface in INTERFACES:
        entries = []
        for owner, ret, name, args in flatten(decls, iface):
            key = (owner, name)
            if key not in fn_of:
                fname, code = gen_method(owner, ret, name, args)
                fn_of[key] = fname
                if code:
                    functions.append(code)
                else:
                    declared.add(fname)
            entries.append((name, len(args) + 1, fn_of[key]))
        tables.append((iface, entries))

    out = []
    out.append("// Généré par tools/d3d9/gen_com.py à partir de d3d9.h (dxvk-native). Ne pas éditer.")
    out.append('#include "rt_d3d9.h"')
    out.append("")
    out.append("#include <d3d9.h>")
    out.append("")
    out.append('extern "C" {')
    out.append("")
    for fname in sorted(declared):
        out.append("uint32_t %s(cpu_t *c, const uint32_t *a);" % fname)
    out.append("")
    out += functions
    out.append("")
    for iface, entries in tables:
        out.append("static const rt_shim_entry d3d9_%s[] = {" % iface)
        for name, nargs, fname in entries:
            out.append('    {"d3d9com::%s::%s", %d, %s},' % (iface, name, nargs, fname))
        out.append("};")
        out.append("")
    out.append("const rt_com_iface rt_com_ifaces[] = {")
    for iface, entries in tables:
        out.append('    {"%s", d3d9_%s, %d},' % (iface, iface, len(entries)))
    out.append("};")
    out.append("const unsigned rt_com_iface_count = %d;" % len(tables))
    out.append("")
    out.append('}  // extern "C"')
    out.append("")
    open(dst, "w", encoding="utf-8").write("\n".join(out))
    print("interfaces : %d, méthodes : %d, fonctions générées : %d, sur mesure : %d"
          % (len(tables), sum(len(e) for _, e in tables), len(functions), len(declared)))


if __name__ == "__main__":
    main()
