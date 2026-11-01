# Passation : Crazy Taxi 3 sur Linux (état au 1er novembre 2026)

Document destiné à un autre agent (ou à une autre personne) qui reprend le projet. Il résume ce qui a été fait,
ce qui est établi, ce qui a été corrigé à tort, les décisions prises, et ce qui reste à faire. Le détail des
décisions est dans `DECISIONS.md` (D-001 à D-050 et notes datées). Ce document ne le remplace pas.

## 1. Objectif et contraintes

- **But** : faire tourner `CT3.exe` (Crazy Taxi 3, PE32 i386, version modifiée 16:9) nativement sous Linux x86_64,
  rendu Vulkan, par **recompilation statique** (x86 vers C) avec un traducteur maison. Le portage Switch est
  reporté (D-011).
- **Contraintes de l'utilisateur** (à respecter) :
  - Ne jamais modifier `CT3.exe` ni `CT3.original.exe` (lecture seule). Ne pas copier les assets du jeu dans le dépôt :
    le jeu les lit depuis `/run/media/stephane/BC250_USB/Games/Crazy Taxi 3/`.
  - Pas de contournement de protection. Aucune protection n'a été rencontrée à ce jour.
  - Machine Bazzite (immuable) : outils via distrobox, Homebrew ou venv utilisateur ; **jamais `rpm-ostree`**.
  - Dépôt de travail : `/var/home/stephane/Dev/ct3-linux` (git, un commit par étape). Notes d'analyse :
    `/var/home/stephane/Dev/ct3/analysis/`. Journal des décisions : `DECISIONS.md`.
  - Captures d'écran : dossier `captures/` (ignoré par git).
  - Autonomie accordée : avancer sans demander, prendre la solution la plus proche des règles de l'art (décompilation,
    émulation), consigner les décisions. Ne solliciter l'utilisateur qu'à une étape claire (ici : menu, vidéo ou son).
- **Machine** : AMD BC-250 (GPU RADV GFX1013, Mesa 26.2.4, Vulkan 1.4). Session Plasma/Wayland. Écran utilisateur :
  `DISPLAY=:0`.

## 2. Arborescence et outils

| Chemin | Rôle |
|---|---|
| `runtime/src/rt_*.c/.cpp` | Runtime : mémoire invitée, CPU, threads, fichiers, fenêtres, DirectX, GDI, tas |
| `runtime/include/rt*.h` | Interfaces du runtime (`rt.h` = cœur : `cpu_t`, `guest_ld/st`, `dispatch`) |
| `runtime/src/rt_shims.c` | Table des imports Win32 (`{"DLL::Nom", nargs, fonction}`), `rt_shim_call`, `rt_unresolved` |
| `runtime/src/rt_extra.cpp` | GDI, DirectSound (stub silencieux), DirectInput (clavier), ADVAPI32, ole32 |
| `runtime/src/rt_win.cpp` | Fenêtres SDL, file de messages Win32, état clavier DIK |
| `runtime/src/rt_d3d9*.cpp`, `rt_d3d9_gen.cpp` | Pont D3D9 vers dxvk-native (wrappers COM invités) |
| `runtime/src/rt_font.c` | Polices hôte (fontconfig + stb_truetype) pour le texte GDI |
| `runtime/src/rt_proc.c` | Processus : TEB/PEB, tas (allocateur à classes de taille), pile de retour, vérification des registres |
| `runtime/src/rt_diag.c` | Diagnostics (sonde d'instructions, espion mémoire) : variables `CT3_*` |
| `runtime/third_party/stb_truetype.h` | Rastériseur de polices, v1.26, domaine public (téléchargé de nothings/stb) |
| `tools/recomp/ct3rc.py` | Traducteur : p-code Ghidra vers C (`dispatch`, `RET`, `CALL`, sauts indirects) |
| `tools/recomp/run_g3.sh`, `cmake/CT3Recomp.cmake` | Export Ghidra, traduction et compilation de la bibliothèque du jeu |
| `tools/ghidra/ExportFunctions.java` | Script Ghidra : export p-code, création de fonctions (modes `pcode-all-ptrs`) |
| `tools/d3d9/gen_com.py` | Génère `rt_d3d9_gen.cpp` à partir de `d3d9.h` |
| `tests/` | `cpu`, `vec`, `x87` (CPU), `g1_d3d9_triangle`, `g2` (chargement), `run` (lanceur `ct3_run`) |
| `build/` | Build normal (Release-like). `build-o0/` : build de traçage `-O0` (`CT3_TRACE_INSN=1`). `build*/` ignoré |
| `build/g3/pcode_all.json` | Export p-code de toutes les fonctions (outil d'analyse principal) |
| `captures/` | Captures PNG (ignorées par git) |

Outils système utilisés : Ghidra 12.1.4 (`~/.local/share/ghidra_12.1.4_PUBLIC`), venv `~/.venvs/ct3recomp`
(capstone, pour désassembler les octets d'origine), Xvfb, `xdotool`, `ImageMagick import`, PIL, `fc-match`.

### Commandes

```sh
cd /var/home/stephane/Dev/ct3-linux
cmake --build build --target ct3_run          # build normal
cmake --build build-o0 --target ct3_run       # build de traçage
rm -f build/g3/libct3gen.a                    # forcer la retraduction après une modification de ct3rc.py
```

Lancer le jeu (test, Xvfb) :
```sh
Xvfb :99 -screen 0 1280x1024x24 -nolisten tcp &
DXVK_FILTER_DEVICE_NAME=llvmpipe DISPLAY=:99 timeout -s KILL 280 ./build/tests/run/ct3_run \
  "/run/media/stephane/BC250_USB/Games/Crazy Taxi 3/CT3.exe" > /dev/null 2> journal.err
```
Sur l'écran utilisateur : `DISPLAY=:0`, sans `DXVK_FILTER_DEVICE_NAME` (RADV).

Tests : `tests/cpu/test_cpu`, `tests/vec/test_vec` (sans argument) ; `tests/g2/test_g2`, `tests/x87/test_x87`
(argument : chemin de `CT3.exe`) ; `tests/g1_d3d9_triangle/g1_d3d9_triangle` (Xvfb + llvmpipe). **Tous passent**
au dernier commit (`0df49a9`).

### Variables d'environnement de diagnostic

| Variable | Effet |
|---|---|
| `CT3_SHOT=N`, `CT3_SHOT_DIR=dir` | Capture de la N-ième présentation (PPM) |
| `CT3_SHOT_EVERY=N` | Capture toutes les N présentations (suivi des écrans) |
| `CT3_KEY_AT=N` | Injecte la touche Entrée à la N-ième présentation (test) |
| `CT3_FILELOG=1` | Journalise ouvertures de fichiers et lectures |
| `CT3_GDIDBG=1` | Journalise chaque texte GDI |
| `CT3_ATLAS_DUMP=f.pgm` | Exporte l'atlas de police (DIB monochrome) |
| `CT3_ALLOCLOG=1` | Journal des allocations et libérations avec appelants du jeu |
| `CT3_HEAPCHK=1` | Vérification de cohérence du tas après chaque opération (lent) |
| `CT3_COMDBG=1` | Signale la réutilisation d'un wrapper COM pour une autre interface |
| `CT3_TRACE_INSN=1` (build `-O0`) | Trace par instruction, vérification des registres |
| `CT3_PROBE=lo-hi` (build `-O0`) | Affiche chaque instruction exécutée dans la plage (registres, pile, FILE) |
| `CT3_RING=N` (build `-O0`) | Après N présentations, garde les 256 dernières instructions et les affiche à un appel non traduit |
| `CT3_MEMWATCH=lo-hi`, `CT3_MEMWATCH_VAL=hex` (build `-O0`) | Affiche chaque écriture dans une plage ou de valeur donnée |
| `CT3_WATCH=adresse` (build `-O0`) | Affiche ECX, argument et état (`[0x7BC334]`) à chaque entrée de fonction |
| `CT3_NOREGCHK=1` (build `-O0`) | Désactive la vérification des registres |

**Attention** : `/tmp` est un tmpfs de 4,8 Go. Les journaux de trace l'ont saturé à plusieurs reprises (échec de
compilation, Xvfb tué). Supprimer les gros journaux. Un redémarrage vide `/tmp` (recréer `/tmp/opencode/ct3`).

**Attention au décompte** : `grep -c "IDirect3DDevice9::Present"` compte les lignes `[import]` **et** `[retour]`.
Pour compter les images, utiliser `grep -c "retour\] d3d9com::IDirect3DDevice9::Present"`.

## 3. Ce qui est établi (vérifié)

### Traduction et runtime (jalons G1 à G3, puis extensions)
- Traduction de toutes les fonctions de `CT3.exe` (5 594 fonctions, export Ghidra p-code, D-018 à D-027).
- x87, MMX, SSE, 3DNow!, CPUID (profil AMD), sauts indirects, setjmp/longjmp (D-020 à D-036).
- Processus : TEB/PEB, TLS, tas, ~250 shims Win32, threads hôte réels avec verrou réentrant (D-031).
- Fichiers Win32 relatifs au répertoire du jeu, insensibles à la casse (D-032).
- Pont D3D9 vers dxvk-native : wrappers COM invités `[vtable][id]` ; méthodes générées depuis `d3d9.h` (D-033).
- Fenêtre SDL dimensionnée au back buffer (D-040). Clavier : messages `WM_KEYDOWN` (SDL) et DirectInput (D-046).
- Tests : tous verts (voir ci-dessus).

### Séquence de démarrage du jeu (observée, `CT3_SHOT_EVERY`)
1. Intros : SEGA, Hitmaker, Empire Interactive, Strangelite (images visibles).
2. Écran titre : logo « Crazy Taxi 3 » avec invite « PRESS START » clignotante (image visible, vérifiée).
3. Après Entrée : écran sombre où seul « PRESS START » reste visible (non résolu, voir section 5).

### Machine d'états principale du jeu (`FUN_0042E140`)
- Dispatcher : état dans `[0x7BC334]`, table de gestionnaires à `0x67F94C` (index : état, valeur 0 = retour).
  Gestionnaires : état 0 `0x42DC20`, 1 `0x42E160`, 2 `0x42DE70` (initialisation audio, `FUN_0043FF80`/`0x43EC20`),
  3 `0x42E330` (titre, attente « PRESS START »), 6 `0x42DF90`, 7 `0x42DFF0`, 8 `0x42E0A0`, 9 `0x42E460`.
- Compteur de trames `[0x717884]`.
- Chaîne observée au démarrage : état 1 pendant 1 024 images, puis chargement des banques son (`FUN_004A0150`),
  état 6, 7 (1 078 images), 2 (initialisation audio), 3 (titre).
- **Entrée** : l'état 3 appelle `FUN_0042DB20`, qui appelle `FUN_004A13B0(-1)`. Cette fonction lit des
  enregistrements de périphériques (pas de 0xCC octets, à `0x7167A2` / `0x71653E`) remplis par DirectInput
  (`FUN_00418860`, callbacks `EnumDevices`). Sans périphérique, « PRESS START » n'est jamais détecté.
  Corrigé par le clavier DirectInput (D-046) : après Entrée, l'état progresse (3, 6, 7, 2, 3...).

### Textes et polices
- Le jeu police : `Arial` (chaîne à `0x53B6F0`). Le nom de police passé à `CreateFontA` n'est pas lisible
  à l'adresse indiquée par l'argument (`0x400010D8`, code) : le code utilise un repli `Arial` quand la chaîne n'est
  pas imprimable. À vérifier si d'autres polices apparaissent.
- Le jeu construit au démarrage un **atlas de police monochrome 256×256** : 95 dessins `ExtTextOutA` dans un DIB
  1 bit (les 95 autres appels n'ont pas de DIB sélectionné et sont ignorés). L'atlas rendu est correct
  (`captures/gdi_atlas_police.png`, ASCII complet en gras).
- **Pas encore vu** : texte de menu à l'écran (voir section 5).

### Audio
- DirectSound est un stub silencieux (D-038). Les banques `Media/SE/taxi0.bin` et `taxi1.bin` (format `xbhm`)
  sont lues entièrement en mémoire (48 Mo pour `taxi1`) quand `[0x67BE78]` est non nul.
- Le jeu ouvre des fichiers `Media/AIFF/*.aif`, `Media/Music/m03.ogg`, `Media/SE/taxi2hot.bin`,
  `Media/PasDat/CT2KC1_0.D00`, `Media/fcvdatalv.bin`, `Media/bank11.mdl`. Aucun son n'est joué.

## 4. Corrections d'analyse (erreurs passées à ne pas refaire)

1. **« 2 050 présentations »** était faux : c'était le double (import + retour). Le nombre réel est d'environ 1 025
   images à la fin de la phase d'attente de 1 024 images. Voir section 2, « Attention au décompte ».
2. **« Le pointeur du tableau de canaux est nul »** était faux : `[élément+0]` vaut `0x264F2B5C`, valide.
   Mesuré directement avec la sonde, après l'avoir déduit à tort du crash.
3. **« La banque ne se charge pas »** était faux : le parseur lit bien les compteurs (11 et 5 éléments). Le
   chargement est normal ; l'initialisation audio `0x43EC20` n'était simplement pas encore exécutée.
4. **« Le plantage vient de la lecture de banque »** était faux : c'était le **tas** (allocateur qui ne réutilisait
   que les blocs de taille identique) puis des **wrappers COM réutilisés** pour une autre interface (D-043, D-045).
5. **Un faux élément factice** (`CT3_HOOK_AUDIO`, retiré) reculait le plantage sans le résoudre : ne pas le réintroduire.
6. **L'espion de pile** et la sonde pouvaient lire des adresses non mappées (faux plantages). Les lectures sont bornées.

## 5. Corrections de bugs réels (commits)

| Commit | Correction |
|---|---|
| `7cb3cd7` | **Traduction** : le corps d'une fonction était émis dans l'ordre des adresses ; si un bloc plus bas (atteint par saut) précédait l'entrée, la fonction exécutait ce bloc (ex. `_strchr` à `0x518830`). Ajout d'un `goto` vers l'entrée (D-041). |
| `7cb3cd7` | **DirectSound 3D** : `QueryInterface` renvoyait le tampon pour tout IID. L'interface 3D (`IDirectSound3DBuffer`, IID vérifié à `0x5CEF4C`) a désormais sa vtable et ses arités (D-042). |
| `c7ad4ca` | **Allocateur de tas** : classes de tailles (puissances de deux), découpe et fusion. Plus de plafond de 4 096 blocs libres (D-043). |
| `b095bfe` | **`wsprintfA`** : `%S` (chaîne large, UTF-16) n'était pas géré (D-044). |
| `f33f1f7` | **Wrappers COM** : une adresse hôte réutilisée pour une autre interface renvoyait l'ancien wrapper (mauvaise vtable). Création d'un nouveau wrapper (D-045). |
| `5435c38` | **Clavier DirectInput** : `EnumDevices` appelle le callback du jeu avec le clavier ; `CreateDevice`, `GetDeviceState` (D-046). |
| `5435c38` | **Trampolines** : `0x451000` est un `jmp` isolé non traduit ; `rt_unresolved` suit les `jmp rel32/rel8` (D-047). À corriger plus tard dans l'export Ghidra. |
| `5435c38` | **Tas** : 192 Mio puis 960 Mio, à `0x40000000` (D-048). |
| `0df49a9` | **Texte GDI** : polices hôte, DC/DIB/police suivis, rendu 32/24/1 bit (D-050). |

## 6. Décisions de conception en vigueur (résumé ; détail dans DECISIONS.md)

- **Traduction** : p-code Ghidra (export `pcode-all-ptrs`) puis C généré par `ct3rc.py` ; `dispatch(c, adresse)` pour
  les appels et sauts hors fonction ; `RET` traité comme saut conditionnel sur une pile de retours par thread (D-014,
  D-039).
- **Registres** : `cpu_t` par thread ; vérification des registres préservés après chaque appel (build de traçage) ;
  exceptions documentées pour les prologues SEH `0x51B390`, `0x519D80`, l'épilogue `0x51B3CB` et le helper 64 bits
  `0x518E20` (qui utilise EBX).
- **Interfaces COM** : wrappers invités, une vtable par interface ; réutilisation d'une adresse hôte pour une autre
  interface = nouveau wrapper (D-045).
- **Tas** : allocateur à classes de tailles ; blocs `[taille][état]` ; vérification `CT3_HEAPCHK` (D-043).
- **Audio** : silencieux pour l'instant (D-038). **Plan** : banques, DirectSound complet, sortie SDL (D-008).
- **Texte GDI** : polices hôte via fontconfig et stb_truetype (D-050). Pas de repli bitmap.
- **Fenêtre** : taille du back buffer (D-040). **Plein écran** : demande `Windowed=0` traitée de la même façon.
- **Dépendance externe** : `stb_truetype.h` v1.26 (domaine public), dans `runtime/third_party/`.

## 7. État actuel et prochaine étape

- Dernier commit : `0df49a9`. Arbre de travail propre à la fin de la session précédente.
- Le jeu tourne au-delà de 4 000 images sans erreur (build normal, Xvfb, llvmpipe).
- **Ce qui manque pour voir le menu** : après « PRESS START » (état 3 → 6 → 7 → 2 → 3), l'écran devient noir avec
  seul « PRESS START » visible. Le menu n'apparaît pas. Hypothèses à vérifier, dans l'ordre :
  1. Un état attend un résultat (audio terminé, fichier chargé, timer) qui ne vient jamais. Suivre
     `[0x7BC334]` et `[0x717884]` après la touche (`CT3_WATCH=42e140` sur un build `-O0` ; l'état 3 dure
     environ 1 000 images).
  2. Le menu est dessiné avec des textures issues de l'atlas ou de DIB que notre pont ne restitue pas
     (vérifier `CreateTexture`/`LockRect` pour les formats du jeu, et l'upload depuis un DIB).
  3. La transition attend un événement (fin de vidéo, de son) : vérifier les `Sleep` et les appels
     `GetCurrentPosition`/`GetStatus` de DirectSound.
- Pistes de travail déjà repérées : `CreateFontA` reçoit un nom de police illisible (vérifier l'argument) ;
  `GetObjectA`, `GetDIBits`, `SetDIBits` non implémentés ; BitBlt absent.

## 8. Pièges connus

- Un crash affiché comme « appel vers une adresse non traduite 0x…58 » signifie souvent une **vtable** erronée
  (mauvaise interface ou objet réutilisé), pas une fonction manquante. Vérifier l'ordre de la vtable.
- Une valeur de retour lue après un appel peut venir d'une **autre case de pile** si le décalage `ESP` est mal suivi :
  vérifier `ESP` avant et après chaque appel (sonde `CT3_PROBE`).
- Les décalages d'`ESP` d'un appel à l'autre peuvent être normaux (profondeur d'appel différente) : comparer
  l'équilibre de la pile sur un même cycle d'appel, pas entre deux appelants.
- `CT3_PROBE=0-fffffffe` sans `CT3_RING` affiche tout et casse l'exécution : utiliser une plage limitée.
- Le build `-O0` est très lent (plusieurs fois plus lent) : les runs de 280 secondes atteignent 2 000 à 3 000 images.
- Après une modification de `ct3rc.py`, supprimer `build*/g3/libct3gen.a` pour régénérer la traduction (plusieurs
  minutes).
- `build*/` et `captures/` ne sont pas versionnés.

## 9. Historique des commits (résumé)

```
0df49a9 Texte GDI : polices hôte (stb_truetype + fontconfig), DC/DIB/police suivis, rendu 32/24/1 bit (D-050)
5435c38 Entrée clavier DirectInput (D-046), trampolines non traduits (D-047), tas 960 Mio (D-048)
f33f1f7 COM : pas de réutilisation d'un wrapper d'une autre interface (D-045)
b095bfe wsprintfA : %S (chaîne large) ; diagnostics tas et ouvertures
c7ad4ca Allocateur de tas à classes de taille (D-043)
7cb3cd7 Goto vers l'entrée (D-041), DirectSound 3D (D-042), tampon d'instructions
c066319 Diagnostic audio : espion mémoire, journal de lecture, sonde enrichie
87db935 Diagnostic audio : sonde CT3_PROBE, surveillance bornée
1a2a5be Diagnostic : CT3_NOREGCHK ; essai de contournement audio (échec, retiré)
d2225c9 Vérification des registres préservés et RET traité comme saut
6d93937 Débordement d'arguments des talons corrigé (16 max)
829d5ae Trace d'instruction (build -O0), protection des blocs du tas
40c03d9 Fenêtre au back buffer, intros affichées
fc4dddd Pont Direct3D 9 (dxvk-native), fenêtres SDL, setjmp/longjmp, tas
bf88f22 Threads réels (option B), fichiers Win32
acd7477 Démarrage depuis le point d'entrée : TEB/PEB, tas, shims
d56be4d Couverture complète : MMX, SSE, 3DNow!, sauts indirects
4ffcf2c à 037ebe8 Référence Unicorn, CPUID, x87
5dcf0eb Gate G3 : traduction de toutes les fonctions
54f36c4 Gate G2 : chargement de CT3.exe
887c9b9 Gate G1 : triangle D3D9 via dxvk-native
```
