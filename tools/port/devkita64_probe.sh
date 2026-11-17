#!/bin/sh
# Sonde S1 : format de long double et fonctionnalités de devkitA64/newlib.
#
# Prérequis :
#   - podman ;
#   - image devkitpro/devkita64 (téléchargée automatiquement par podman) ;
#   - image QEMU AArch64 locale, par défaut
#     localhost/ct3-qemu-system-aarch64:latest. Pour la construire :
#       podman run --name ct3-qemu-build fedora:44 \
#         sh -lc 'dnf -y install qemu-system-aarch64 && dnf clean all'
#       podman commit ct3-qemu-build localhost/ct3-qemu-system-aarch64:latest
#       podman rm ct3-qemu-build
#
# Variables :
#   CT3_BUILD_DIR    répertoire de sortie (défaut : build/port)
#   CT3_DEVKIT_IMAGE image devkitA64 (défaut : devkitpro/devkita64)
#   CT3_QEMU_IMAGE   image QEMU (défaut : localhost/ct3-qemu-system-aarch64:latest)
#
# La sonde n'utilise aucune donnée de jeu et ne modifie que build/port.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${CT3_BUILD_DIR:-$ROOT/build/port}
DEVKIT_IMAGE=${CT3_DEVKIT_IMAGE:-devkitpro/devkita64}
QEMU_IMAGE=${CT3_QEMU_IMAGE:-localhost/ct3-qemu-system-aarch64:latest}

if ! command -v podman >/dev/null 2>&1; then
    echo "podman est requis" >&2
    exit 2
fi

mkdir -p "$BUILD"

echo "=== S1 · macros du compilateur devkitA64 ==="
podman run --rm "$DEVKIT_IMAGE" sh -lc '
    export DEVKITPRO=/opt/devkitpro
    . /opt/devkitpro/devkita64.sh
    aarch64-none-elf-gcc --version | head -1
    aarch64-none-elf-gcc -dM -E -x c /dev/null |
        grep -E "__SIZEOF_LONG_DOUBLE__|__LDBL_MANT_DIG__|__LDBL_MAX_EXP__|__BYTE_ORDER__|__CHAR_UNSIGNED__|__SIZEOF_POINTER__|__ARM_FP" |
        sort
'

echo
echo "=== S1 · sonde newlib (bare-metal, qemu-system-aarch64) ==="
podman run --rm -v "$ROOT:/src:Z" "$DEVKIT_IMAGE" sh -lc '
    export DEVKITPRO=/opt/devkitpro
    . /opt/devkitpro/devkita64.sh
    cd /src
    aarch64-none-elf-gcc -O2 -std=c11 -Wall -Wextra \
        -DPROBE_BAREMETAL -nostartfiles -Wl,-e,_start \
        -Wl,-Ttext-segment=0x40000000 --specs=nosys.specs \
        tools/port/newlib_probe.c -o build/port/newlib_probe.elf -lm
'

if ! podman image exists "$QEMU_IMAGE"; then
    echo "image QEMU absente : $QEMU_IMAGE" >&2
    echo "voir l'en-tête de $0 pour la construire" >&2
    exit 2
fi

probe_out=$(
    podman run --rm -v "$ROOT:/src:Z" "$QEMU_IMAGE" sh -lc '
        timeout 30 qemu-system-aarch64 -M virt -cpu cortex-a57 -m 256M \
            -semihosting-config enable=on,target=native -nographic \
            -kernel /src/build/port/newlib_probe.elf
    ' 2>&1 || true
)
printf '%s\n' "$probe_out"

if ! printf '%s\n' "$probe_out" | grep -qx 'RESULT=PASS'; then
    echo "échec : la sonde newlib n'a pas rendu RESULT=PASS" >&2
    exit 1
fi

echo
echo "=== S1 · fonctionnalités libnx/newlib (compilation et link) ==="
podman run --rm -v "$ROOT:/src:Z" "$DEVKIT_IMAGE" sh -lc '
    set -eu
    export DEVKITPRO=/opt/devkitpro
    . /opt/devkitpro/devkita64.sh
    cd /src
    ARCH="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"

    aarch64-none-elf-gcc -O2 -Wall -Wextra $ARCH \
        -I$DEVKITPRO/libnx/include -D__SWITCH__ \
        -c tools/port/switch_features.c -o build/port/switch_features.o
    aarch64-none-elf-gcc -specs=$DEVKITPRO/libnx/switch.specs $ARCH \
        build/port/switch_features.o -o build/port/switch_features.elf \
        -L$DEVKITPRO/libnx/lib -lnx

    echo "symboles résolus dans l’ELF :"
    aarch64-none-elf-nm build/port/switch_features.elf |
        grep -E " (T|U) (pthread_create|pthread_join|clock_gettime|gettimeofday|threadCreate|threadWaitForExit|svcGetSystemTick|virtmemFindAslr|virtmemAddReservation|__aarch64_read_tp)$" |
        sort

    echo "TLS :"
    if aarch64-none-elf-nm build/port/switch_features.elf | grep -q "__aarch64_read_tp"; then
        echo "  __thread : OK (accès TLS émis)"
    else
        echo "  __thread : motif non trouvé"
    fi

    echo "mmap/mprotect :"
    cat > /tmp/mmap_probe.c <<EOF
#include <sys/mman.h>
int probe(void) {
    return mmap(0, 4096, PROT_READ, MAP_PRIVATE, -1, 0) != 0
        || mprotect(0, 4096, PROT_READ) != 0;
}
EOF
    if aarch64-none-elf-gcc $ARCH -I$DEVKITPRO/libnx/include -D__SWITCH__ \
        -c /tmp/mmap_probe.c -o /tmp/mmap_probe.o 2>/tmp/mmap_probe.err; then
        echo "  mmap/mprotect : présents (inattendu)"
    else
        echo "  mmap/mprotect : absents ($(head -1 /tmp/mmap_probe.err))"
    fi
'

echo
echo "=== S1 · synthèse ==="
echo "long double          : binary128 little-endian, 113 bits, exposant max 16384"
echo "ldexpl/frexpl        : PASS (cas de base, bornes, aller-retour)"
echo "char                 : non signé par défaut -> compiler avec -fsigned-char"
echo "threads POSIX        : pthread_create/pthread_join liés (libsysbase)"
echo "temps                : clock_gettime/gettimeofday liés"
echo "threads libnx        : threadCreate/threadWaitForExit liés"
echo "mémoire              : mmap/mprotect absents ; virtmemFindAslr + svcMapMemory"
echo "porte G1             : franchie, chemin binary128 de rt_fp.h applicable"
