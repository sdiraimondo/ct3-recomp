# TODO : Crazy Taxi 3 sous Linux

Liste de travail, dans l'ordre de priorité indiqué par l'utilisateur. Le détail technique de chaque point est dans
`DECISIONS.md` ; l'état d'ensemble est dans [`HANDOFF.md`](HANDOFF.md). Le plan consolidé et traçable — chantiers
Linux `L1`–`L12`, port Switch `S1`–`S10`, portes `G1`–`G6`, critères d'acceptation — est dans
[`ROADMAP.md`](ROADMAP.md). Le point de situation courant est consigné dans
[`POINT_SITUATION.md`](POINT_SITUATION.md). Mise à jour : 18 nov. 2026.

## Terminé ou en place

- **Démarrage lent (écran noir)** : premier affichage à environ 9 à 10 s (contre 43 à 45 s avant les portages), sur
  l'écran de l'utilisateur comme en Xvfb. Routines portées en code natif, vérifiées bit à bit contre la traduction
  (`CT3_FILTER_CHECK`, `CT3_FILTER_CHECK_DITHER`) : filtre `FUN_004DACC6` (mode 1), tramage `FUN_004DDB6F` et
  `FUN_004DD45C`, alpha DXT3 `FUN_004F0D8B`, ajustement des extrémités `FUN_004EFA9B`. Voir `docs/PORTAGE_FILTRE.md`
  et D-068 à D-070.
- **Écran de chargement vidéo (D-070)** : `avi/loading.avi` est lue au lancement (FFmpeg, build LGPL), en plein écran,
  fond noir, vidéo à sa taille d'origine et centrée. La fenêtre du jeu reste cachée pendant la vidéo et s'affiche au
  premier affichage du jeu. Vérifié sur l'écran de l'utilisateur.
- **Manettes** : premier jet fait (D-064, D-065), testé avec une DualShock 4.
- **Son** : premier jet fait (D-067). Bruitages et musique fonctionnent.

## 1. Manettes : reste à faire

- Vérifier les touches de conduite en jeu (klaxon, Crazy Dash) et la touche de validation des menus (Start ou L1).
- Corriger l'effet d'un appui qui semble déplacer le curseur de deux entrées.
- v1.1 : modifier les paramètres des manettes dans l'interface du jeu (et écrire `Config.ini`).
- Hors liste : étendre `Config.ini` (résolution, autres réglages).

## 2. Son : reste à faire

- **Atténuation 3D (premier jet fait)** : position/min/max distance stockées dans `IDirectSound3DBuffer`, atténuation
  par distance et panoramique basique dans le mixeur SDL (`rt_extra.cpp`). À vérifier en jeu et affiner (facteur de
  chute, cônes, orientation, position de l'écouteur non renseignée par le jeu pour l'instant).
- Interpolation du rééchantillonnage (plus proche voisin actuellement ; une version linéaire a été tentée puis retirée
  à cause d'accès mémoire hors bornes).
- Cinématiques : muettes, faute de flux DirectShow (D-061).

## 3. Ralentissements en jeu (compilation des shaders) : à traiter

- Constat : des à-coups en jeu, aléatoires. Non mesurés. La compilation des shaders et des pipelines au premier usage
  est la piste principale.
- Pistes :
  - Vérifier que le cache DXVK (`~/.cache/dxvk/*.dxvk.bin`) est écrit et relu d'un lancement à l'autre.
  - Compilation asynchrone des pipelines (option DXVK, D-013).
  - Préchauffage pendant l'écran titre, ou précompilation à partir du cache.
- À mesurer d'abord : temps passé dans la compilation (journal DXVK ou `perf`).

## 4. Démarrage : reste à faire

- Le bloc de couleur DXT3 `FUN_004F0019` (chemin `param_2 == 0`) est encore traduit. Sa structure est décrite dans
  `docs/PORTAGE_FILTRE.md`, section 11. Gain attendu plus modeste que les précédents.
- Les autres modes du filtre (2 et 3) ne sont pas portés (non vérifiés). Le décodeur DXT1 natif reste désactivé.
- Après le premier affichage, le jeu montre encore son logo en fondu (images sombres vers les images 260 et 500 à 600,
  mesure Xvfb). Couvrir ces fondus par la vidéo est une décision à prendre.

## 5. Win32 : manques et implémentations partielles

- Sur 203 imports de `CT3.exe`, 191 sont implémentés. 12 manquent : `CreateFileW`, `EnumSystemLocalesA`,
  `GetEnvironmentStrings`, `GetLocaleInfoW`, `RaiseException`, `RtlUnwind`, `DialogBoxParamA`, `EndDialog`,
  `GetDlgItem`, `IsDlgButtonChecked`, `CheckRadioButton`, `GetIconInfo`. Les ordinaux `COMCTL32 #17` et `DSOUND #11`
  sont couverts. Un appel à l'un des 12 arrête le runtime (code 3) ; aucun n'apparaît dans nos journaux, sites d'appel
  non vérifiés.
- Attention : des stubs naïfs de `RaiseException`/`RtlUnwind` (sortie immédiate, unwind sans effet) cassent la gestion
  SEH/C++ du jeu et provoquent un crash après l'écran de chargement. Ne pas les réintroduire tels quels ; il faut une
  vraie gestion des exceptions ou laisser l'import absent.
- Implémentations partielles : `GetStringType` (type 1 seulement), `LCMapString` (indicateurs 0x100 et 0x200
  seulement).
- Licence : le dépôt n'a pas encore de licence. Tant que ce n'est pas tranché, aucun code de Wine ne peut être copié.
  Une réimplémentation à partir de la documentation et des tests de Wine reste possible.

## 6. Cinématiques

- **Fait** : les cinématiques sont lues par FFmpeg et envoyées directement au filtre « INTRO TEXTURE RENDERER » du jeu (Receive) en passant par un graphe DirectShow factice. Le décodeur est ouvert au Run, la texture est fournie par le jeu, les images BGR24 sont converties en BGRA avec le bon pas et retournées verticalement (DIB bas-en-haut). EC_COMPLETE n'est posté qu'à la fin réelle (ou immédiatement si le fichier manque) pour ne pas figer la scène SA. Rewind/loop fonctionne (`put_CurrentPosition(0.0)`). Audio des cinématiques non implémenté (hors point 2).
- Tous les fichiers dans `avi/` sont présents : `loading.avi` (rawvideo 128×128), cinématiques titre (`go_*.avi`, `wc_*.avi`, `sa_*.avi`) en cinepak 640×480 et replay (`Movie_LV.avi`, `Movie_SA.avi`) en rawvideo 128×128. Vérifié en lecture : wc_ax/wc_bd.
- Reste à faire : tests des cinématiques en jeu (sélection de voiture, replay), vérifier la synchronisation avec les états du réalisateur, et éventuellement corriger d'éventuels problèmes de format.

## 7. Runtime sans CT3.exe (lots 1 et 2 faits)

- **Lot 1 fait (D-073)** : `tools/pepack/pepack.py` et la cible `ct3pack` produisent `build/pack/`
  (`ct3_data.bin`, `ct3_image.h`, `ct3_image.json`). `ct3pack-check` vérifie les 4 574 adresses statiques du C
  généré (`packed=4572`, `excluded=0`, `gap=0`). Format et contrat de chargement : `docs/PACK.md`.
- **Lot 2 fait (D-074)** : le runtime charge le pack, plus `CT3.exe`.
  - `runtime/src/rt_pe.c` : lecture de `ct3_data.bin`, zéro-remplissage du BSS, sections, imports dans l'ordre,
    point d'entrée `CT3_IMAGE_ENTRY`, protections sans exécution.
  - `.text` est incluse dans le pack **comme données** : le code traduit y lit des tables de sauts
    (`0x0051B19C`, entre autres). Sans elle, le jeu s'arrête sur un appel à `0x00000000` au démarrage.
  - `runtime/src/rt_shims.c` : `rt_unresolved` ne suit les trampolines que si les octets de `.text` sont
    présents (`CT3_IMAGE_HAS_TEXT`).
  - `tests/run/test_run.c` et `tests/common/harness.h` : plus de chemin de `CT3.exe` ; dossier du jeu optionnel
    ou `CT3_DATA_DIR`, pack cherché automatiquement (`CT3_PACK` pour le forcer).
  - `runtime/src/rt_file.c` et `runtime/src/rt_splash.c` : le dossier des assets vient de `rt_set_game_dir`,
    plus du chemin de l'exécutable.
- **Critère vérifié (4 nov. 2026)** : `ct3_run` sans argument depuis le dossier du jeu, avec `CT3.exe` masqué
  par un montage vide : 987 présentations en 30 s, aucune erreur. Avec `CT3_DATA_DIR` : 1 893 présentations en
  60 s. Aucun `open`/`fopen` de `CT3.exe` observé par intercepteur `LD_PRELOAD`. Tests `cpu`, `vec`, `g2`, `x87`
  et `g1_d3d9_triangle` verts.

## Constats à traiter (hors liste de l'utilisateur)

### Plantage A (D-069) : faute à 0x3F800000 au bout de 5 à 12 min de course

- Revenu sur le build avec portages natifs (3 nov., 301 s de course), même signature : `EBX = EBP = 0x45183AD4`,
  fonction `FUN_00414ED0`. Le plantage précède les portages, mais rien n'exclut une écriture native.
- Écrivain de la valeur fautive : non identifié.
- À faire : si reproductible, comparer avec `CT3_FILTER_NATIVE=0`.

### Plantage à la sortie du jeu (adresse non traduite 0x005154b5)

- En quittant le jeu, le journal affiche `appel vers une adresse non traduite 0x005154b5`. Fonction de la table
  `atexit`, non désassemblée.
- Impact : seulement la sortie. Sans effet sur le jeu en cours.
- À faire : repérer les écritures de pointeurs de fonction dans `atexit` / `_onexit`.

### Intros d'introduction : encore buggées

- Constat utilisateur (2 nov.) : les cinématiques d'introduction restent défectueuses. Quelle intro, quel moment, quel
  défaut : à relever avec une capture au moment du bug (`CT3_SHOT`).
- Lien possible avec le graphe DirectShow factice (D-061).

### Personnages déformés dans les cinématiques

- Constat utilisateur : polygones manquants ou décalés pour les personnages (LV et SA), décor et véhicules corrects.
- Piste (non vérifiée) : palette de matrices de squelette (`VERTEXBLEND`), calcul de matrices `FUN_004E7813`.
- À faire : capture au moment du défaut, puis comparaison.

### « Tas invité épuisé » après environ 20 minutes

- Un run s'est arrêté à 17:45 par `rt: tas invité épuisé`.
- À faire : `CT3_ALLOCLOG=1` et `CT3_HEAPCHK=1` sur une session longue, pour trouver la fuite.

### Crash à la fin d'une manche/round
- Observé de manière intermittente à la fin d'une partie (temps écoulé). À reproduire et tracer (stack trace sous gdb).
- Peut être lié à la libération/atexit ou à un état de jeu non géré.

### Crashes DXVK/Vulkan intermittents
- `VK_ERROR_DEVICE_LOST` observé avec DXVK (notamment sur BC-250/RADV). Peut provoquer un abort du processus.
- À investiguer : options DXVK, gestion d'erreur, stabilité sur ce matériel.

### Écran noir intermittent
- Écran noir observé ponctuellement au démarrage/transitions. Généralement passager, à surveiller et tracer si récurrent.
