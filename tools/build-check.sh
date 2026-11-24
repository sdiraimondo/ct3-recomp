#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${CT3_EXE:-}" ]; then
    case "$CT3_EXE" in
        /*) ;;
        *) CT3_EXE="$PWD/$CT3_EXE" ;;
    esac
fi
cd "$ROOT"
BUILD_DIR=${CT3_BUILD_DIR:-"$ROOT/build-check"}
case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$ROOT/$BUILD_DIR" ;;
esac
GENERATOR=${CT3_BUILD_GENERATOR:-Ninja}
DEPS_PREFIX=${CT3_DEPS_PREFIX:-"$HOME/.local/ct3-deps/prefix"}
UNICORN_PYTHON=${CT3_UNICORN_PYTHON:-"$HOME/.venvs/unicorn-ref/bin/python"}

if [ ! -x "$UNICORN_PYTHON" ]; then
    echo "erreur : Python Unicorn introuvable : $UNICORN_PYTHON" >&2
    echo "définir CT3_UNICORN_PYTHON vers un interpréteur contenant Unicorn" >&2
    exit 1
fi

if ! "$UNICORN_PYTHON" -c 'import unicorn' >/dev/null 2>&1; then
    echo "erreur : Unicorn indisponible dans $UNICORN_PYTHON" >&2
    exit 1
fi

if [ -n "${CT3_EXE:-}" ]; then
    if [ ! -f "$CT3_EXE" ]; then
        echo "erreur : CT3_EXE n'est pas un fichier : $CT3_EXE" >&2
        exit 1
    fi
    if [ ! -f "$DEPS_PREFIX/lib64/libdxvk_d3d9.so" ]; then
        echo "erreur : dxvk-native introuvable dans $DEPS_PREFIX/lib64" >&2
        exit 1
    fi
    if [ ! -d "$DEPS_PREFIX/include/dxvk" ]; then
        echo "erreur : en-têtes dxvk-native introuvables dans $DEPS_PREFIX/include/dxvk" >&2
        exit 1
    fi
    cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
        -DCT3_EXE="$CT3_EXE" \
        -DCT3_DEPS_PREFIX="$DEPS_PREFIX" \
        -DCT3_UNICORN_PYTHON="$UNICORN_PYTHON"
    cmake --build "$BUILD_DIR" --target test_cpu test_vec test_g2 test_x87 g1_d3d9_triangle
else
    cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
        -DCT3_EXE= \
        -DCT3_DEPS_PREFIX="$DEPS_PREFIX" \
        -DCT3_UNICORN_PYTHON="$UNICORN_PYTHON"
    cmake --build "$BUILD_DIR" --target test_cpu test_vec g1_d3d9_triangle
fi

"$BUILD_DIR/tests/cpu/test_cpu"
"$BUILD_DIR/tests/vec/test_vec"

if [ -n "${CT3_EXE:-}" ]; then
    CT3_PACK="$BUILD_DIR/pack" "$BUILD_DIR/tests/g2/test_g2"
    CT3_PACK="$BUILD_DIR/pack" "$BUILD_DIR/tests/x87/test_x87"
fi

if ! command -v xvfb-run >/dev/null 2>&1; then
    echo "erreur : xvfb-run requis pour le test graphique G1 sans écran" >&2
    exit 1
fi
xvfb-run -a env DXVK_FILTER_DEVICE_NAME=llvmpipe \
    "$BUILD_DIR/tests/g1_d3d9_triangle/g1_d3d9_triangle"

"$ROOT/tools/check_no_game_data.sh"
