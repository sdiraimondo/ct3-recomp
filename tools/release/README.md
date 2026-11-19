# ct3-linux @VERSION@ — bundle local

Binaire Linux x86-64 de **Crazy Taxi 3** recompilé : runtime ct3-linux + code du
jeu traduit en C. Le pack de données et les assets ne sont **pas** inclus.

> **Usage local uniquement.** Le binaire lie le code traduit de *votre* copie de
> `CT3.exe` : c'est un dérivé du jeu, qui ne doit être ni redistribué, ni publié,
> ni joint à un rapport. Le pack (`ct3_data.bin`, `ct3_image.h`) est également un
> dérivé et se génère localement.

## Prérequis

- Linux x86-64 avec un pilote Vulkan 1.3 (testé avec Mesa RADV sur AMD BC-250) ;
- une copie de Crazy Taxi 3 (le `CT3.exe` **et** le dossier d'assets `avi/`,
  `Media/`, etc.) ;
- Python 3 pour générer le pack (aucun module externe).

## Démarrage rapide

```sh
tar xf ct3-linux-@VERSION@-x86_64.tar.gz
cd ct3-linux-@VERSION@-x86_64

# 1. Générer le pack depuis VOTRE copie (le SHA-256 est vérifié)
./bin/ct3-mkpack "/chemin/vers/Crazy Taxi 3/CT3.exe"

# 2. Lancer le jeu
./ct3-linux "/chemin/vers/Crazy Taxi 3"
```

Sans argument, le lanceur utilise le dossier courant (ou `CT3_DATA_DIR`) comme
dossier du jeu. Le pack est cherché dans `share/ct3/pack/`, puis via `CT3_PACK`,
puis à côté du binaire et dans le dossier courant.

Si `CT3.exe` n'est pas celui qui a servi à construire ce bundle, `ct3-mkpack`
refuse le fichier : les constantes de sections et d'imports sont compilées dans
le runtime. Le SHA-256 attendu est dans `share/ct3/EXPECTED_SHA256`.

## Contenu

| Chemin | Rôle |
|---|---|
| `ct3-linux` | lanceur (bibliothèques locales, pack, arguments) |
| `bin/ct3_run` | exécutable (Release, code du jeu lié statiquement) |
| `bin/ct3-mkpack` | génération du pack depuis `CT3.exe` |
| `lib/` | SDL2, DXVK 3.1.1, FFmpeg (LGPL), dépendances non système |
| `share/ct3/pepack.py` | outil de pack (code du projet) |
| `share/ct3/EXPECTED_SHA256` | empreinte du `CT3.exe` attendu |
| `share/ct3/pack/` | **vide** : destination du pack généré |
| `LICENSES/` | licences des composants redistribués |
| `RELEASE.json` | version, commit, compilateur, empreinte du `CT3.exe` |
| `SHA256SUMS` | empreintes de tous les fichiers du bundle |

## Réglages utiles

- `Config.ini` (dossier courant, ou `CT3_CONFIG`) : réglages du jeu, mapping
  manette, `[Patches] SkipIntro=1`, `[Game] Aspect=16:9`, etc.
- `CT3_WINDOWED=1` : force une swapchain fenêtrée (tests, Xvfb).
- `CT3_NOAUDIO=1` : désactive le son.
- `CT3_PACK=/chemin/ct3_data.bin` : force le pack.
- `CT3_DATA_DIR=/chemin/jeu` : dossier des assets.
- `CT3_HEAPSTATS=1` : rapport de tas toutes les 30 s (`CT3_HEAPSTATS_MS`).
- `CT3_ALLOCSTATS=1` : profil des blocs vivants par site appelant.
- `CT3_VIRTUALFREE_NOOP=1` : témoin A/B de l'ancien `VirtualFree` (diagnostic).

## Vérification

```sh
sha256sum -c SHA256SUMS
```

Le binaire a été construit en `Release` avec le commit et le compilateur
indiqués dans `RELEASE.json`. Il utilise le `libstdc++` et la glibc de la
machine de build ; pour une distribution vers une base plus ancienne, il
faudrait lier ou embarquer ces bibliothèques et rebuild avec la toolchain
correspondante.

## Limites connues

- Le test de stabilité de 60 minutes (L1.3) n'est pas encore clos : le correctif
  des arènes `VirtualAlloc` est inclus, mais la session longue reste à valider.
- Les cinématiques, les ralentissements et les réglages du menu du jeu restent
  des chantiers ouverts (voir `docs/ROADMAP.md` du dépôt).
- La vérification sémantique fonction par fonction (`V1`–`V4`) n'est pas
  terminée ; elle ne bloque pas l'exécutable.

## Mentions

Voir `LICENSES/NOTICE.md`. Le code original du projet est sous
GPL-3.0-or-later (D-082) ; ce bundle reste destiné à un usage local car il
lie le code traduit du jeu, qui est un dérivé de votre copie de `CT3.exe`.
Il ne doit pas être publié.
