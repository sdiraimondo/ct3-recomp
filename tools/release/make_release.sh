#!/bin/sh
# Assemble un bundle de release local à partir d'un build Release.
#
# Le bundle contient le binaire, les bibliothèques non système et les outils de
# génération du pack. Il ne contient NI CT3.exe, NI pack, NI code C généré :
# ces artefacts sont des dérivés du jeu et se génèrent localement.
#
# Usage :
#   tools/release/make_release.sh [--build-dir DIR] [--version V] [--out DIR]
#
# Prérequis : un build configuré avec
#   -DCT3_RPATH='$ORIGIN/../lib' -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
# et la cible ct3_run construite.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build_dir=${CT3_RELEASE_BUILD:-$root/build-release}
version=${CT3_RELEASE_VERSION:-0.1.1-linux}
out_root=${CT3_RELEASE_OUT:-$root/dist}
deps=${CT3_DEPS_PREFIX:-$HOME/.local/ct3-deps/prefix}

while [ $# -gt 0 ]; do
    case $1 in
        --build-dir) build_dir=$2; shift 2 ;;
        --version) version=$2; shift 2 ;;
        --out) out_root=$2; shift 2 ;;
        -h|--help)
            sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "make_release : option inconnue : $1" >&2
            exit 2
            ;;
    esac
done

bin=$build_dir/tests/run/ct3_run
pack_json=$build_dir/pack/ct3_image.json
name=ct3-linux-$version-x86_64
stage=$out_root/$name

[ -x "$bin" ] || {
    echo "make_release : binaire absent : $bin" >&2
    echo "  construire avec -DCT3_RPATH='\$ORIGIN/../lib' -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON" >&2
    exit 1
}
[ -f "$pack_json" ] || {
    echo "make_release : manifeste du pack absent : $pack_json" >&2
    exit 1
}

# Vérifie que le binaire est relocalisable (sinon le bundle ne l'est pas).
if ! readelf -d "$bin" | grep -q 'RUNPATH.*\$ORIGIN'; then
    echo "make_release : avertissement : RUNPATH sans \$ORIGIN dans $bin" >&2
fi

rm -rf "$stage"
mkdir -p "$stage/bin" "$stage/lib" "$stage/share/ct3/pack" \
         "$stage/share/doc/ct3-linux" "$stage/LICENSES"

install -m 0755 "$bin" "$stage/bin/ct3_run"
strip --strip-unneeded "$stage/bin/ct3_run" 2>/dev/null || true

copy_lib() {
    src=$1
    dst=$2
    if [ ! -f "$src" ]; then
        echo "make_release : bibliothèque absente : $src" >&2
        exit 1
    fi
    cp -L "$src" "$dst"
}

copy_lib "$deps/lib64/libSDL2-2.0.so.0"      "$stage/lib/libSDL2-2.0.so.0"
copy_lib "$deps/lib64/libdxvk_d3d9.so.0"     "$stage/lib/libdxvk_d3d9.so.0"
copy_lib "$deps/lib/libavformat.so.63"       "$stage/lib/libavformat.so.63"
copy_lib "$deps/lib/libavcodec.so.63"        "$stage/lib/libavcodec.so.63"
copy_lib "$deps/lib/libswscale.so.10"        "$stage/lib/libswscale.so.10"
copy_lib "$deps/lib/libavutil.so.61"         "$stage/lib/libavutil.so.61"

install -m 0755 "$root/tools/release/ct3-linux" "$stage/ct3-linux"
install -m 0755 "$root/tools/release/ct3-mkpack" "$stage/bin/ct3-mkpack"
install -m 0644 "$root/tools/pepack/pepack.py" "$stage/share/ct3/pepack.py"
cp "$root/tools/release/licenses/"*.txt "$stage/LICENSES/"
cp "$root/tools/release/licenses/NOTICE.md" "$stage/LICENSES/NOTICE.md"

pack_sha=$(python3 - "$pack_json" <<'PY'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    print(json.load(f)["source"]["sha256"])
PY
)
printf '%s\n' "$pack_sha" > "$stage/share/ct3/EXPECTED_SHA256"

sed "s/@VERSION@/$version/g" "$root/tools/release/README.md" > "$stage/README.md"

commit=$(git -C "$root" rev-parse HEAD)
short=$(git -C "$root" rev-parse --short HEAD)
commit_date=$(git -C "$root" show -s --format=%cI HEAD)
build_date=$(date -u +%Y-%m-%dT%H:%M:%SZ)
cc=$(sed -n 's/^CMAKE_C_COMPILER:FILEPATH=//p' "$build_dir/CMakeCache.txt")
cc_version=$("$cc" --version 2>/dev/null | head -n 1 || echo unknown)

python3 - "$stage/RELEASE.json" "$version" "$commit" "$short" "$commit_date" \
        "$build_date" "$cc_version" "$pack_sha" <<'PY'
import json, sys
out, version, commit, short, commit_date, build_date, cc, pack_sha = sys.argv[1:]
data = {
    "name": "ct3-linux",
    "version": version,
    "arch": "x86_64",
    "build_type": "Release",
    "commit": commit,
    "commit_short": short,
    "commit_date": commit_date,
    "build_date": build_date,
    "compiler": cc,
    "pack_source_sha256": pack_sha,
    "distribution": "local-only",
    "notes": (
        "Le binaire lie le code du jeu traduit (libct3gen.a), derive de "
        "CT3.exe : usage local uniquement, ne pas redistribuer. Le pack de "
        "donnees et les assets ne sont pas inclus."
    ),
}
with open(out, "w", encoding="utf-8") as f:
    json.dump(data, f, ensure_ascii=False, indent=2)
    f.write("\n")
PY

if LD_LIBRARY_PATH="$stage/lib" ldd "$stage/bin/ct3_run" | grep -q "not found"; then
    echo "make_release : dépendance non résolue :" >&2
    LD_LIBRARY_PATH="$stage/lib" ldd "$stage/bin/ct3_run" | grep "not found" >&2
    exit 1
fi

(
    cd "$stage"
    find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)

mkdir -p "$out_root"
tarball=$out_root/$name.tar.gz
tar -C "$out_root" -czf "$tarball" "$name"
(
    cd "$out_root"
    sha256sum "$name.tar.gz" > "$name.tar.gz.sha256"
)

echo "release : $stage"
echo "archive : $tarball"
echo "sha256  : $(cut -d' ' -f1 "$out_root/$name.tar.gz.sha256")"
du -sh "$stage" "$tarball"
