# Publier

Ce document décrit ce qui peut être publié et ce qui ne doit jamais l'être.
Le projet publie du **code source et des outils**, pas le jeu.

## Principe

Le binaire `ct3_run` lie `libct3gen.a`, la traduction du code de `CT3.exe`.
C'est une œuvre dérivée du jeu : **le bundle local n'est pas publiable**, même
s'il ne contient ni assets ni pack. Le pack et le C généré sont également des
dérivés.

La publication se fait donc **source seule** :

- code du runtime, du recompilateur et des outils ;
- documentation, scripts de build, licences et notices ;
- archives source générées par l'hébergeur (GitHub, etc.).

L'utilisateur fournit son propre `CT3.exe`, génère le pack et le code traduit
localement, puis construit son binaire. Voir `README.md`.

## Ce qui peut être publié

| Élément | Publiable ? | Remarque |
|---|---|---|
| `runtime/`, `tools/`, `cmake/`, `tests/` | oui | code original, GPL-3.0-or-later |
| `docs/`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `LEGAL.md` | oui | documentation et notices |
| `tools/deps/build_ffmpeg.sh` | oui | script de build FFmpeg |
| `tools/port/*.patch` | oui | patchs originaux, DXVK zlib marqué |
| `LICENSES/` | oui | licences tierces |
| archive source GitHub | oui | si le dépôt est propre |

## Ce qui ne doit jamais être publié

| Élément | Pourquoi |
|---|---|
| `bin/ct3_run` / `build/tests/run/ct3_run` | lie le code traduit du jeu |
| `libct3gen.a`, `gen_funcs*.c`, `gen_dispatch.c`, `pcode.json` | traduction du code du jeu |
| `ct3_data.bin`, `ct3_image.h`, `ct3_image.json` | octets du jeu (en-têtes, sections, ressources) |
| `CT3.exe`, `CT3.original.exe`, DLL | code du jeu |
| `avi/`, `Media/`, textures, sons, polices | assets du jeu |
| `dist/` | bundle local, dérivé du jeu |
| captures, vidéos, journaux avec assets | contenu du jeu |
| clés, liens vers une copie non autorisée | illégal ou contraire à l'éthique |

Le `.gitignore` couvre déjà `dist/`, `ct3_data.bin`, `ct3_image.*` et
`/pack/`. Le script `tools/check_no_game_data.sh` vérifie l'arbre **suivi** et
l'historique ; exécutez-le avant chaque publication.

## Checklist de release source

1. `git status` est propre ; aucun fichier de jeu n'est suivi.
2. `tools/check_no_game_data.sh` passe (arbre + historique).
3. `LICENSE`, `THIRD_PARTY.md`, `LEGAL.md`, `CONTRIBUTING.md` et `LICENSES/`
   sont présents et à jour.
4. La CI est verte.
5. Le `README.md` indique la version, les prérequis et l'absence de données de
   jeu.
6. Le tag est annoté (`git tag -a vX.Y.Z -m ...`) et pointe sur un commit dont
   l'arbre est propre.
7. La page de release ne contient **aucun binaire**, aucune capture et aucun
   asset du jeu ; elle renvoie vers les instructions de build.
8. `dist/` n'est pas joint à la release.
9. En cas de doute juridique, faire relire la release avant publication.

Exemple de note de release :

```text
Source-only release. This repository contains no game code, no assets and no
recompiled output. You need your own legally obtained copy of Crazy Taxi 3 for
PC. Build instructions: README.md. The local release bundle (dist/) is not
published because it links translated game code.
```

## Bundle local

`tools/release/make_release.sh` assemble un bundle pour **usage local** :

- il contient `bin/ct3_run`, qui lie le code traduit ;
- il ne contient ni `CT3.exe`, ni pack, ni assets ;
- `RELEASE.json` porte `"distribution": "local-only"` ;
- il ne doit pas être téléversé, joint à une release ou partagé.

Le générateur de pack `bin/ct3-mkpack` et les outils sont publiables ; le
binaire final ne l'est pas.

## Publication d'un binaire sans code du jeu (option future)

Un binaire du runtime **sans code traduit** pourrait être publié après :

- découplage de `libct3gen` en bibliothèque construite localement et chargée
  par le runtime ;
- suppression des constantes de jeu codées en dur (lecture depuis le pack) ;
- construction contre une glibc ancienne ou dans un runtime portable
  (AppImage/Flatpak), car le binaire actuel exige `GLIBC_2.43` ;
- conformité LGPL de FFmpeg (liaison dynamique, source, remplacement) ;
- revue juridique du runtime et de ses shims spécifiques au jeu.

Tant que ces points ne sont pas faits, la publication reste **source seule**.

## Voir aussi

- `LEGAL.md` : limites juridiques et reverse engineering.
- `THIRD_PARTY.md` : licences des composants.
- `CONTRIBUTING.md` : contributions et DCO.
- `README.md` : construction et exécution.
