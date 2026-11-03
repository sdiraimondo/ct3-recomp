#!/bin/sh
# Construit FFmpeg (version LGPL, sans composant GPL) dans le préfixe des dépendances du projet.
# Utilisé par l'écran de chargement (avi/loading.avi). Nécessite : git, make, un compilateur C.
# Usage : tools/deps/build_ffmpeg.sh [préfixe]   (défaut : $HOME/.local/ct3-deps/prefix)
set -eu

PREFIX="${1:-$HOME/.local/ct3-deps/prefix}"
TAG="${FFMPEG_TAG:-n9.0.2}"
SRC="${FFMPEG_SRC:-${TMPDIR:-/tmp}/ct3-ffmpeg-src}"

if [ ! -d "$SRC/.git" ]; then
    rm -rf "$SRC"
    git clone --depth 1 --branch "$TAG" https://github.com/FFmpeg/FFmpeg.git "$SRC"
fi

cd "$SRC"
# --disable-x86asm : pas besoin de nasm (build plus lent à l'exécution, suffisant pour un écran de chargement).
./configure \
    --prefix="$PREFIX" \
    --enable-shared --disable-static \
    --disable-x86asm --disable-programs --disable-doc --disable-debug --disable-network \
    --disable-everything \
    --enable-avformat --enable-avcodec --enable-swscale --enable-avutil \
    --enable-demuxer=avi \
    --enable-parser=mpegvideo,mpeg4video,h264,mjpeg \
    --enable-protocol=file \
    --enable-decoder=rawvideo,mjpeg,mpeg1video,mpeg2video,mpeg4,msmpeg4v2,msmpeg4v3,wmv1,wmv2,wmv3,vc1,cinepak,indeo3,indeo4,indeo5,msvideo1,svq1,svq3,h264,pcm_s16le,pcm_u8,adpcm_ima_wav,adpcm_ms,mp3,vorbis,ac3

make -j"$(nproc)"
make install
echo "FFmpeg installé dans $PREFIX (LGPL 2.1 ou ultérieure)."
