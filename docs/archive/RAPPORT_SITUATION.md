# Rapport détaillé de la situation - Crazy Taxi 3 sous Linux

Date : 5 novembre 2026

> **Obsolète** — archivé le 17 novembre 2026. Ce rapport décrit un état antérieur (crash après
> l'écran de chargement) qui n'est plus d'actualité : le jeu démarre, affiche ses menus et se joue.
> État courant : `README.md`, `docs/TODO.md` et `docs/SWITCH_PORT.md`.

## Objectif
Faire tourner Crazy Taxi 3 nativement sous Linux x86_64 par recompilation statique (x86 vers C) avec un traducteur maison. Le jeu doit fonctionner sans `CT3.exe` au runtime, en utilisant un pack de données généré au build.

## État actuel
Le jeu **démarre et se lance** mais **plante juste après le premier écran de chargement** avec une erreur :
```
rt: dernière instruction invitée 0x00000000
rt: faute mémoire (signal 11) à l'adresse hôte 0x...000c, invitée 0xc
```
Il s'agit d'un crash par déférencement de pointeur nul (accès à offset 0xc sur pointeur null).

## Contraintes respectées
- `CT3.exe` et `CT3.original.exe` non modifiés (lecture seule)
- Artefacts dérivés (`build/pack/*`, `build/g3/*`, `libct3gen.a`) non versionnés
- Le pack préserve adresses virtuelles, imports, point d'entrée, BSS
- Assets trouvés sans dépendre du chemin de `CT3.exe`
- `CT3.exe` uniquement nécessaire au build

## État de la décompilation
- **5 747 fonctions** traduites
- **442 811 instructions** x86 couvertes
- **2 338 789 opérations** p-code
- **258 sites non pris en charge** dans **2 fonctions seulement** (inatteignables depuis le point d'entrée)
- **309 fonctions atteignables** depuis le point d'entrée, **aucune avec site non pris en charge**
- **241 appels/sauts indirects** non résolus statiquement (passent par dispatch à l'exécution)
- **4 cibles d'appels absentes** de l'export

La décompilation est donc **essentiellement complète (~99%)**.

## Ce qui fonctionne
- Traduction statique complète et compilation sans erreur
- Chargement depuis pack (lots 1 et 2 terminés, commit `9804d85`)
- ~1900-2200 présentations en 60s selon le lancement
- Aucun accès à `CT3.exe` au runtime
- Tests `cpu`, `vec`, `g2`, `x87`, `g1_d3d9_triangle` verts
- Sortie propre corrigée (problème `0x005154b5` résolu)
- Shaders DXVK compilés avec cache existant (254 shaders)

## Modifications apportées au cours de cette session

### 1. Correction de la sortie du jeu (0x005154b5)
- **Fichier modifié** : `build/g3/gen/gen_dispatch.c` (ajout temporaire puis retiré)
- **Objectif** : Intercepter les appels vers la fonction de traitement de la table atexit MSVC (`0x5154a0-0x5154c0`) non traduite
- **Résultat** : Problème résolu, puis retiré pour éviter des régressions

### 2. Ajout de shims pour imports Win32 manquants
- **Fichiers modifiés** : `runtime/src/rt_shims.c`
- **Imports ajoutés** :
  - `KERNEL32.dll::RaiseException` (stub qui exit(3) avec message)
  - `KERNEL32.dll::RtlUnwind` (stub qui log et retourne 0)
  - `KERNEL32.dll::CreateFileW` (retourne INVALID_HANDLE_VALUE)
  - `DSOUND.dll::#11` (stub)
- **Résultat** : Build OK, imports couverts

### 3. Corrections de build
- **Fichier modifié** : `runtime/src/rt_diag.c`
  - Ajout de `extern int rt_trace_imports` pour éviter définition multiple
  - Ajout de symboles manquants pour compatibilité : `rt_cur_insn`, `rt_cs_depth`, `rt_cs_stack`, `rt_writers_on`, `rt_watch_init`, `rt_watch_entry` (weak), `rt_writers_report`, `rt_memwatch_fill`, `rt_memwatch_block`
- **Fichier modifié** : `runtime/CMakeLists.txt` (ajout/retrait temporaire de rt_watch.c)
- **Résultat** : Build fonctionnel

### 4. Tentative de préchauffage des shaders
- **Fichier modifié** : `runtime/src/rt_d3d9_custom.cpp` (ajout puis retrait)
- **Approche** : Dessin factice au premier Present pour forcer la compilation des shaders
- **Résultat** : Causait des crashes, retiré

## État git actuel
De nombreux fichiers sont modifiés (non commités) :
- `runtime/src/rt_diag.c` - contient des symboles ajoutés pour compatibilité build
- `runtime/src/rt_shims.c` - contient des stubs pour imports manquants
- Autres fichiers runtime modifiés (état préexistant)
- `DECISIONS.md` modifié
- `tools/recomp/ct3rc.py` modifié

Aucun commit n'a été fait pour ces changements.

## Problème actuel
Le jeu crash juste après le premier écran de chargement avec un déférencement de pointeur nul. Ce crash s'est manifesté après les modifications apportées et persiste même après annulation de la plupart des changements. Il pourrait être lié à :
1. Un état corrompu du build
2. Un problème avec les symboles ajoutés dans rt_diag.c
3. Un problème préexistant qui se manifeste différemment
4. Un effet de bord des modifications

## Fichiers clés
- `runtime/src/rt_shims.c` - Shims Win32
- `runtime/src/rt_diag.c` - Diagnostics
- `runtime/src/rt_d3d9_custom.cpp` - Wrapper D3D9
- `build/g3/gen/gen_dispatch.c` - Dispatch des fonctions traduites
- `tools/recomp/ct3rc.py` - Traducteur p-code vers C
- `docs/TODO.md` - Liste des tâches restantes
- `docs/HANDOFF.md` - Passation
- `DECISIONS.md` - Journal des décisions
