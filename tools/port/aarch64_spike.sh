#!/bin/sh
# Spike de portabilité AArch64 du runtime CT3, sans toolchain croisée sur l'hôte.
#
# Compile et exécute test_x87, test_cpu et test_vec dans un conteneur arm64
# (podman + binfmt). Le dépôt n'est pas modifié : les sources sont copiées
# dans un répertoire temporaire monté dans le conteneur.
#
# Prérequis :
#   - un build Linux existant avec les cibles x87, pack et vec ;
#   - podman et le support binfmt aarch64 (qemu-user-static).
#
# Usage :
#   tools/port/aarch64_spike.sh
#   CT3_BUILD_DIR=/chemin/build tools/port/aarch64_spike.sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=${CT3_BUILD_DIR:-"$root/build"}

for f in "$build/x87/gen/gen_funcs_000.c" "$build/g2/gen/gen_funcs_000.c" "$build/pack/ct3_data.bin" "$build/vec/vectors.h"; do
    if [ ! -f "$f" ]; then
        echo "manque $f" >&2
        echo "construire d'abord : cmake --build \"$build\" --target test_x87 test_g2 ct3pack test_vec" >&2
        exit 1
    fi
done

if ! command -v podman >/dev/null 2>&1; then
    echo "podman est requis" >&2
    exit 1
fi

out=${CT3_AARCH64_OUT:-"${TMPDIR:-/tmp}/ct3-aarch64-spike"}
mkdir -p "$out"

cat > "$out/stubs.c" <<'STUBS'
/* Stubs minimaux pour isoler les tests du runtime D3D9/SDL2. */
#include "rt.h"
#include "rt_pe.h"

unsigned rt_shim_calls;
uint32_t rt_shim_last_arg0;

/* Pile d'appels invitée : le code généré la consulte. */
__thread uint32_t rt_cs_stack[RT_CS_MAX];
__thread unsigned rt_cs_depth;

/* Suivi mémoire (rt_diag.c) : inutile pour ces tests. */
uint32_t rt_memwatch_lo, rt_memwatch_span, rt_memwatch_val;
void rt_memwatch_hit(cpu_t *c, uint32_t addr, unsigned size, uint64_t v)
{
    (void)c; (void)addr; (void)size; (void)v;
}
void rt_memwatch_fill(uint32_t addr, int value, unsigned n) { (void)addr; (void)value; (void)n; }
void rt_memwatch_block(uint32_t addr, const void *src, unsigned n) { (void)addr; (void)src; (void)n; }
void rt_watch_entry(cpu_t *c, uint32_t addr) { (void)c; (void)addr; }

void rt_set_image(rt_pe_image *img) { (void)img; }
void rt_set_image_has_text(int has_text) { (void)has_text; }

void rt_shim_call(cpu_t *c, uint32_t stub)
{
    (void)stub;
    rt_shim_calls++;
    /* CloseHandle (stdcall, 1 argument) : argument à ESP+4, puis « ret 4 »
     * dépile l'adresse de retour et l'argument. */
    rt_shim_last_arg0 = (uint32_t)guest_ld(c, (uint32_t)cpu_rd(c, 16, 4) + 4, 4);
    cpu_wr(c, 16, 4, cpu_rd(c, 16, 4) + 8);
    cpu_wr(c, 0, 4, 1); /* succès */
}

void rt_unresolved(cpu_t *c, uint32_t addr)
{
    (void)c;
    if (addr == RT_HOST_RET) return; /* retour vers l'appelant C */
    rt_fatal("fonction invitée non résolue");
}
STUBS

cat > "$out/inner.sh" <<'INNER'
set -eu
apk add --no-cache gcc musl-dev >/dev/null

rm -rf /out/work
mkdir -p /out/work
cp -r /src/runtime /src/tests /out/work/
cp -r /build/x87/gen /out/work/gen
cp -r /build/g2/gen /out/work/gen_g2
cp -r /build/pack /out/work/pack
cp -r /build/vec /out/work/vec
cp /out/stubs.c /out/work/

cd /out/work
CFLAGS="-O2 -std=gnu11 -fno-strict-aliasing -Iruntime/include -Iruntime/src -Igen -Ipack -Itests/common"

gcc $CFLAGS /src/tools/port/f80_crosscheck.c -lm -o f80_crosscheck

gcc $CFLAGS \
    runtime/src/rt_mem.c runtime/src/rt_pe.c runtime/src/rt_cpu.c \
    runtime/src/rt_vec.c runtime/src/rt_jmp.c \
    gen/gen_funcs_000.c gen/gen_dispatch.c stubs.c tests/x87/test_x87.c \
    -lm -o test_x87_arm64

gcc $CFLAGS -Igen_g2 \
    runtime/src/rt_mem.c runtime/src/rt_pe.c runtime/src/rt_cpu.c \
    runtime/src/rt_vec.c runtime/src/rt_jmp.c \
    gen_g2/gen_funcs_000.c gen_g2/gen_dispatch.c stubs.c tests/g2/test_g2.c \
    -lm -o test_g2_arm64

gcc $CFLAGS \
    tests/cpu/test_cpu.c runtime/src/rt_mem.c runtime/src/rt_cpu.c stubs.c \
    -lm -o test_cpu_arm64

gcc $CFLAGS -Ivec \
    tests/vec/test_vec.c runtime/src/rt_vec.c \
    -lm -o test_vec_arm64

echo "=== f80_crosscheck ==="
./f80_crosscheck
echo "=== test_x87 ==="
CT3_PACK=/out/work/pack/ct3_data.bin ./test_x87_arm64
echo "=== test_g2 ==="
CT3_PACK=/out/work/pack/ct3_data.bin ./test_g2_arm64
echo "=== test_cpu ==="
./test_cpu_arm64
echo "=== test_vec ==="
./test_vec_arm64
INNER

exec podman run --rm --arch arm64 --security-opt label=disable \
    -v "$root":/src:ro \
    -v "$build":/build:ro \
    -v "$out":/out \
    docker.io/library/alpine:3.20 sh /out/inner.sh
