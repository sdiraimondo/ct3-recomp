# Composants tiers

Ce dépôt contient du code original sous **GPL-3.0-or-later** (voir `LICENSE`),
mais il dépend de composants tiers. Chacun reste sous sa propre licence ; la
GPL du projet ne les relicencie pas.

## Composants liés ou embarqués

| Composant | Version | Licence | Fichier | Modifications |
|---|---|---|---|---|
| [SDL2](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10) | 2.32.10 | zlib | `LICENSES/SDL2.txt` | aucune |
| [DXVK](https://github.com/doitsujin/dxvk) (build `dxvk-native`, D3D9) | 3.1.1, commit `21fda8f` | zlib | `LICENSES/DXVK.txt` | WSI Switch, patch `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch` |
| [FFmpeg](https://github.com/FFmpeg/FFmpeg/tree/n9.0.2) (avformat, avcodec, swscale, avutil) | n9.0.2, build LGPL | LGPL-2.1-or-later | `LICENSES/FFmpeg-LGPL-2.1.txt` | configuration restreinte, voir ci-dessous |
| [stb_truetype](https://github.com/nothings/stb) | v1.26 | domaine public ou MIT | `LICENSES/stb_truetype.txt` | aucune |

### DXVK / dxvk-native

DXVK est utilisé dans sa configuration `native` (Linux), avec un backend
Vulkan. La version de référence est **3.1.1**, commit amont `21fda8f`.

Le port Switch applique des modifications de WSI (surface `VK_NN_vi_surface`,
fenêtre/moniteur, modes 720p/1080p) : le patch reproductible est
`tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch`. Conformément à la licence zlib,
ces versions modifiées sont **marquées comme telles** et ne sont pas présentées
comme du DXVK amont. Le binaire Linux du bundle local peut être construit à
partir de cet arbre modifié ; pour une publication, reconstruire depuis le
commit amont + le patch versionné.

### FFmpeg (LGPL)

FFmpeg est lié **dynamiquement** (`--enable-shared --disable-static`) et
compilé sans composant GPL par `tools/deps/build_ffmpeg.sh`. La configuration
active uniquement les composants nécessaires à l'écran de chargement
(démuxeur AVI, décodeurs vidéo/audio, `avformat`, `avcodec`, `swscale`,
`avutil`).

La LGPL-2.1 exige que l'utilisateur puisse remplacer la bibliothèque :

- les `libav*.so` sont dans `lib/` du bundle local et le lanceur place ce
  dossier en tête de `LD_LIBRARY_PATH` ;
- les sources exactes sont disponibles au tag `n9.0.2` du dépôt FFmpeg, avec
  le script de build du dépôt ;
- aucun code FFmpeg n'est copié dans le code du projet.

Pour une distribution de binaires, joindre le texte LGPL, l'indication de la
version et du script de build, et une offre de source écrite (ou un lien
stable vers les sources correspondantes). La licence LGPL ne couvre pas
d'éventuels brevets sur certains codecs ; ce point est distinct et doit être
évalué séparément.

## Outils de build (non distribués)

| Outil | Version | Licence | Rôle |
|---|---|---|---|
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | 12.1.4 | Apache-2.0 | export p-code à la compilation |
| JDK | 21 | GPL-2.0 avec Classpath Exception | exécution de Ghidra |
| CMake / Ninja | — | BSD / Apache-2.0 | build |
| Python | 3 | PSF | `tools/recomp`, `tools/pepack` |

Ces outils ne sont pas liés au binaire distribué et ne sont pas inclus dans le
dépôt. Les scripts d'export et de traduction du dépôt sont du code original.

## Dépendances système

Le reste (glibc, `libstdc++`, `libgcc_s`, zlib, chargeur Vulkan, pilote
graphique, serveur X/Wayland) est fourni par le système et n'est pas
redistribué. Le bundle local inclut uniquement les bibliothèques listées dans
le premier tableau.

## Code de référence, pas de code copié

Le projet s'appuie sur des faits et des comportements documentés par
[taxipatch](https://github.com/stashymane/taxipatch) (GPL-3.0) et
[CT3Tweaks](https://github.com/stashymane/ct3tweaks) (Apache-2.0), sans
réutiliser leur code. Si du code de ces projets était introduit, il devrait
être documenté ici et rester sous sa licence d'origine ; la combinaison avec
la GPL-3.0-or-later serait compatible pour taxipatch (GPL-3.0) et pour
CT3Tweaks (Apache-2.0), mais chaque fichier concerné conserverait sa notice.

## Voir aussi

- `LICENSE` : GPL-3.0-or-later du code du projet.
- `LEGAL.md` : limites juridiques, jeu, marques, reverse engineering.
- `PUBLISHING.md` : ce qui peut être publié et ce qui ne doit jamais l'être.
