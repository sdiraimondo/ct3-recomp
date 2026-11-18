# Mentions et licences

## Bundle ct3-linux

Ce bundle est destiné à un **usage local**. Il contient `bin/ct3_run`, qui lie
statiquement le code du jeu traduit en C (`libct3gen.a`), dérivé de la copie de
`CT3.exe` de l'utilisateur. Ce binaire est donc un dérivé du jeu : il ne doit
pas être redistribué, publié, ni joint à un rapport de bogue.

Le pack de données (`ct3_data.bin`, `ct3_image.h`) n'est pas inclus. Il se
génère localement avec `bin/ct3-mkpack` depuis la copie de l'utilisateur, et
reste soumis à la même règle : aucun dérivé du jeu n'est distribué.

Les assets (`avi/`, `Media/`, etc.) ne sont pas inclus et restent lus depuis le
dossier du jeu.

## Composants redistribués

| Composant | Version | Licence | Fichier |
|---|---|---|---|
| SDL2 | 2.32.10 | zlib | `SDL2.txt` |
| DXVK (dxvk-native, D3D9) | 3.1.1 | zlib | `DXVK.txt` |
| FFmpeg (avformat, avcodec, swscale, avutil) | n9.0.2, build LGPL | LGPL 2.1 ou ultérieure | `FFmpeg-LGPL-2.1.txt` |

FFmpeg est lié dynamiquement (`lib/libav*.so.63`) et compilé sans composant GPL
(`tools/deps/build_ffmpeg.sh` du dépôt). La licence LGPL 2.1 exige de pouvoir
remplacer ces bibliothèques : elles sont dans `lib/`, et le lanceur place ce
dossier en tête de `LD_LIBRARY_PATH`. Les sources de FFmpeg sont disponibles
aux mêmes conditions que le projet amont (tag `n9.0.2`).

Le reste des dépendances (glibc, libstdc++, libgcc_s, zlib, pilote Vulkan) est
fourni par le système et n'est pas redistribué.

## Projet ct3-linux

La licence du dépôt ct3-linux n'est pas encore tranchée (jalon L11 de la
roadmap). Ce bundle ne constitue pas une décision de licence.
