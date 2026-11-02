#!/usr/bin/env bash
# Gate G3 : export p-code de toutes les fonctions de CT3.exe, traduction en C,
# compilation (sans exécution), rapport chiffré.
#
# Sorties (dans build/g3, non versionnées : dérivées du binaire du jeu) :
#   pcode_all.json   export Ghidra
#   gen/             C généré, chunks gen_funcs_NNN.c, gen_dispatch.c
#   obj/             objets compilés
#   gen/rapport.md   rapport de traduction
#
# Variables : CT3_EXE, GHIDRA_DIR, JAVA_HOME, CC, JOBS, REUSE_EXPORT=1 (réutilise l'export existant).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CT3_EXE="${CT3_EXE:?CT3_EXE doit désigner votre copie de CT3.exe}"
GHIDRA_DIR="${GHIDRA_DIR:-$HOME/.local/share/ghidra_12.1.4_PUBLIC}"
JAVA_HOME="${JAVA_HOME:-}"
CC="${CC:-clang}"
JOBS="${JOBS:-$(nproc)}"
ENTRY="0x518a9a"  # point d'entrée de CT3.exe (en-tête PE)
OUT="${OUT:-$ROOT/build/g3}"

mkdir -p "$OUT/ghidra" "$OUT/gen" "$OUT/obj"
if [ -n "$JAVA_HOME" ]; then
  export JAVA_HOME
  export PATH="$JAVA_HOME/bin:$PATH"
fi

if [ "${REUSE_EXPORT:-0}" != "1" ] || [ ! -s "$OUT/pcode_all.json" ]; then
  echo "[g3] export p-code (Ghidra headless)"
  rm -rf "$OUT/ghidra/ct3"
  "$GHIDRA_DIR/support/analyzeHeadless" "$OUT/ghidra" ct3 \
    -import "$CT3_EXE" \
    -scriptPath "$ROOT/tools/ghidra" \
    -postScript ExportFunctions.java pcode-all-ptrs "$OUT/pcode_all.json" \
    -deleteProject > "$OUT/export.log" 2>&1
fi

echo "[g3] traduction p-code vers C"
python3 "$ROOT/tools/recomp/ct3rc.py" "$OUT/pcode_all.json" "$OUT/gen" --chunk 300 --entry "$ENTRY"

echo "[g3] compilation ($CC, $JOBS tâches)"
set +e
ls "$OUT"/gen/gen_funcs_*.c "$OUT/gen/gen_dispatch.c" | xargs -P "$JOBS" -I{} sh -c \
  "$CC -c -O1 -w -I'$OUT/gen' -I'$ROOT/runtime/include' '{}' -o '$OUT/obj/'\$(basename '{}' .c).o" \
  > "$OUT/compile.log" 2>&1
STATUS=$?
set -e
OBJS=$(ls "$OUT"/obj/*.o 2>/dev/null | wc -l)
if [ $STATUS -eq 0 ]; then
  rm -f "$OUT/libct3gen.a"
  ar rcs "$OUT/libct3gen.a" "$OUT"/obj/*.o
fi
echo "[g3] objets : $OBJS, erreurs de compilation : $([ $STATUS -eq 0 ] && echo 0 || echo "voir $OUT/compile.log")"
cat "$OUT/gen/rapport.md"
exit $STATUS
