# Portage Nintendo Switch — état, mesures et plan

> Statut : note de travail, 18 novembre 2026. Spike AArch64 et sonde devkitA64 (S1) faits.
> Objet : consigner le spike AArch64, les correctifs déjà appliqués et le plan de portage.
> Ce document ne remplace pas `DECISIONS.md` (D-011 : backend séparé, Switch reporté) ; il le met à jour.
> Le plan consolidé, avec les jalons `S1`–`S10`, les portes `G1`–`G6` et la matrice de traçabilité,
> est dans [`ROADMAP.md`](ROADMAP.md) ; les jalons `M0`–`M5` ci-dessous en sont la vue macroscopique.

## 1. Verdict

La **traduction** n'est plus le facteur limitant : ~5 700 fonctions traduites, ~443k instructions,
309 fonctions atteignables, aucun site p-code non supporté sur les chemins atteignables.

Pour autant, la **décompilation n'est pas close** : la complétude du projet est définie par la
vérification sémantique fonction par fonction (chantier `V1`–`V4`, `ROADMAP.md` §6, D-080). Ce
chantier est **hors du chemin critique** des exécutables Linux et Switch et avance en parallèle.

Le portage Switch est un **projet de portage du runtime** ; il ne reprend pas la décompilation et
ne la clôt pas :

- le recompilateur produit du C, recompilé par le compilateur de la cible (ici devkitA64) ;
- le pack de données est indépendant de l'architecture ;
- `CT3.exe` reste un artefact de build, jamais une dépendance d'exécution ;
- le code généré est le même que celui vérifié par `V*` : la vérification s'applique au port
  Switch sans travail supplémentaire, et le port ne bloque pas la vérification.

`mesa-switch` fournit du Vulkan natif homebrew (NVK sur GM20B, WSI `VK_NN_vi_surface` avec scanout
direct par `NWindow`). Le chemin graphique réaliste est donc **DXVK Native + un backend WSI Switch**,
et non une réécriture du renderer en deko3d.

## 2. Spike AArch64 — mesures

Environnement : conteneur `alpine:3.20` arm64 via `podman` + binfmt, gcc 13.2.1, musl.
C'est du **Linux AArch64**, pas Horizon : ce spike valide l'ISA et la couche C, pas la toolchain Switch.

| Test | x86-64 | AArch64 avant | AArch64 après |
|---|---|---|---|
| `test_x87` | OK | **31 échecs / 31 assertions** | **OK, 0 échec** |
| `test_cpu` | OK | OK | OK |
| `test_vec` | OK | OK | OK |
| `test_g2` | OK | non exécuté | OK (stub de shim) |
| `g1_d3d9_triangle` | OK (revérifié) | non exécuté | non exécuté |

### Cause racine

Sur AArch64, `long double` est du binary128 :

```
sizeof(long double)=16  LDBL_MANT_DIG=113  LDBL_MAX_EXP=16384
x87 1.0 décodé par memcpy = 1.95694e-4942
```

`rt_fget(..., 10)` et `rt_fput(..., 10, ...)` faisaient un `memcpy` de 10 octets entre la mémoire
invitée (format étendu x87) et `long double`. C'est exact sur x86-64 (80 bits), faux ailleurs :
les 31 assertions x87 tombaient à zéro.

### Correctif

`runtime/include/rt_fp.h` :

- `rt_f80_get` / `rt_f80_put` : conversion explicite 80 bits x87 ↔ `long double`, avec exposant,
  dénormalisés, infinis et NaN ;
- chemin `memcpy` conservé quand `LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384` (x86), donc
  **aucun changement de comportement ni de performance sur x86-64** ;
- sur binary128 (AArch64), correspondance directe : les deux formats ont les mêmes 15 bits
  d'exposant et le même biais (16383), la mantisse x87 se place en tête des 113 bits par un
  décalage de 49. Aucun appel à `ldexpl`/`frexpl`, donc aucune dépendance à la libm de newlib ;
- repli générique (`ldexpl`/`frexpl`) conservé pour les autres formats de `long double` ;
- arrondi au plus proche, pair en cas d'égalité, à l'encodage.

Validation croisée du chemin rapide (AArch64) : 1 000 000 de motifs x87 valides aller-retour
exacts, 200 000 motifs comparés bit à bit à l'implémentation générique de référence, 200 000
motifs binary128 aléatoires encodés à l'identique → **0 échec**.

`tests/common/harness.h` : `st_set` / `st_get` passent par `rt_fput` / `rt_fget` au lieu de
`memcpy` sur `long double`.

`runtime/include/rt.h` : `rt_rdtsc()` a un repli `clock_gettime(CLOCK_MONOTONIC)` en nanosecondes
sur non-x86. Le port Horizon devra le remplacer par `armGetSystemTick()` pour conserver une échelle
de cycles ; à vérifier selon l'usage qu'en fait le jeu.

`tests/cpu/dispatch_stub.c` : `test_cpu` lie tout `ct3_rt` et manquait `dispatch` sans code généré.
Le stub local satisfait le symbole ; le test se relie et passe à nouveau.

Validation complémentaire : aller-retour 80 bits exact sur `+0`, `-0`, `1`, `-1`, `0.5`, `2`,
plus petit dénormalisé, plus grand fini, `±inf`, `±NaN` → **OK** (le bit de signe des infinis et
des NaN est conservé).

`test_g2` passe aussi sur AArch64 avec un stub de shim : chargement du pack, `_strrchr`, copie
mémoire invitée et appel `CloseHandle` (convention `ret 4` vérifiée). Cela couvre la sémantique
des octets et des pointeurs 32 bits, indépendamment de D3D9 ; la couche Win32 réelle n'est pas
exercée par ce stub.

### Limite du spike

Le spike valide AArch64/LP64 avec `long double` binary128 (glibc/musl). Il **ne valide pas**
devkitA64/newlib. Premier test à faire sur la toolchain Switch :

1. `sizeof(long double)`, `LDBL_MANT_DIG`, `LDBL_MAX_EXP` ;
2. si le format est binary128 little-endian, le chemin rapide s'applique sans aucune fonction
   libm ; sinon, le repli générique exige `ldexpl` / `frexpl` sur des valeurs x87 extrêmes ;
3. si newlib est en `long double` = `double` (53 bits), il faudra un type logiciel `extFloat80`
   et non `long double` : le correctif actuel ne suffira pas.

### Sonde devkitA64 (S1) — 17 novembre 2026

Environnement : image `devkitpro/devkita64`, gcc 15.2.0, newlib, libnx. La sonde
bare-metal est exécutée par `qemu-system-aarch64` (`-M virt -cpu cortex-a57`) ;
les fonctionnalités libnx sont vérifiées à la compilation et au link. Reproduction :
`tools/port/devkita64_probe.sh` (aucune donnée de jeu).

| Mesure | Résultat |
|---|---|
| `sizeof(long double)` | 16 |
| `LDBL_MANT_DIG` | 113 |
| `LDBL_MAX_EXP` / `LDBL_MIN_EXP` | 16384 / −16381 |
| Boutisme | little-endian |
| Précision | au-delà du double (`1.0L + LDBL_EPSILON != 1.0L`) |
| `ldexpl` / `frexpl` | PASS : cas de base, signes, zéro, infini, bornes, aller-retour sur 33 exposants |
| `char` | non signé (`CHAR_MIN = 0`) |
| `pthread_create` / `pthread_join` | liés (libsysbase) |
| `clock_gettime` / `gettimeofday` | liés |
| `threadCreate` / `threadWaitForExit` (libnx) | liés |
| `mmap` / `mprotect` / `munmap` | absents, pas de `sys/mman.h` |
| `__thread` | OK, accès TLS émis (`-mtp=soft`) |

**Porte G1 franchie** : newlib est binary128 little-endian, le chemin rapide de
`rt_fp.h` s'applique sans type `extFloat80` ni `ldexpl`/`frexpl` pour la conversion
x87. Deux conséquences pour le build Switch : `-fsigned-char` est obligatoire
(le code invité suppose `char` signé), et la mémoire invitée ne peut pas utiliser
`mmap`/`mprotect` : il faudra `virtmemFindAslr` + `svcMapMemory` (ou une arène
contiguë réservée au démarrage).

## 3. Ce qui reste bloquant

| Blocage | Poids | État / détail | Prochaine action |
|---|---|---|---|
| WSI DXVK Switch | **critique** | `rt_win.cpp:145` force `DXVK_WSI_DRIVER=SDL2`. DXVK 3.1.1 (commit `21fda8f`) a des backends SDL2/SDL3/GLFW/win32, aucun Switch. `mesa-switch` expose `VK_NN_vi_surface` + `NWindow`, sans copie CPU. | Ajouter `src/wsi/switch/*`, `DXVK_WSI_SWITCH`, `SwitchWSI` ; `createSurface` via `vkCreateViSurfaceNN` ; rendre DXVK loaderless. |
| Vulkan loaderless | **critique** | `src/vulkan/vulkan_loader.cpp` fait `LoadLibraryA("libvulkan.so")`. mesa-switch est statique, sans loader. | Constructeur `LibraryLoader(PFN_vkGetInstanceProcAddr)` et injection de `vkGetInstanceProcAddr` au démarrage. |
| Mémoire invitée | **critique** | `RT_GUEST_SIZE = 1<<32` (`rt.h:12`), `mmap` de 4 GiB (`rt_mem.c:17`), heap `0x40000000` + 960 MiB (`rt_proc.h:15-16`), accès partout via `rt_mem_base + addr`. Horizon n'a ni overcommit ni pagination à la demande ; `mmap`/`mprotect` sont absents de newlib/libnx (S1). | Pic mesuré et stabilisé à 161 Mio (D-081) ; abstraire `rt_va_reserve/commit/protect` ; dimensionner l'arène (planification 512 Mio) ; utiliser `virtmemFindAslr` + `svcMapMemory` sur devkitA64. |
| Threads | moyen | `pthread_cancel` (`rt_thread.c:235`), TLS `__thread`, threads invités. `pthread_create`/`pthread_join` et `__thread` sont disponibles (S1), mais `pthread_cancel` reste à remplacer. | Backend thread Horizon + annulation coopérative ; tests de fin de partie et de sortie. |
| Performance | **élevé** | Conversion x87 par décalage de mantisse sur binary128 (aucune libm) ; mesure indicative sous qemu-user : ~208 ns par paire put+get, contre ~1 264 ns avec `ldexpl`/`frexpl` (émulation binary128 dominante, non représentatif du natif). 4× Cortex-A57 ~1 GHz contre le x86 actuel. | Profiler sur matériel ; overrides natifs `rt_filter.c` si le rendu reste limité par les conversions. |
| Dépendances | moyen | fontconfig absent ; FFmpeg à cross-compiler ; SDL2 Switch incertain pour Vulkan. | Embarquer une police métrique (Liberation Sans) ; FFmpeg minimal ou cinématiques désactivées au premier bring-up ; backend fenêtre NWindow. |
| Build Switch | moyen | Toolchain devkitA64 15.2.0 disponible (S1) ; linkage statique ; pas de `dlopen` ; `char` non signé par défaut. | `cmake/toolchains/switch.cmake`, `-fsigned-char`, `-mtp=soft`, `-fPIE`, tests de fumée. |
| Hygiène des tests | — | Corrigé : `test_cpu` manquait `dispatch` sans code généré ; `tests/cpu/dispatch_stub.c` satisfait le symbole. Le test se relie et passe. | — |
| Juridique | — | Pas de SDK Nintendo ni de code NDA. mesa-switch MIT, DXVK zlib, libnx BSD/Apache. Aucun asset ni code de jeu versionné. | Conserver le contrat du pack : `CT3.exe` fourni par l'utilisateur, pack généré localement. |

### Détail WSI DXVK

Fichiers à toucher dans l'arbre DXVK local (`~/.local/ct3-deps/src/dxvk`) :

- `src/wsi/wsi_platform.h` : `DXVK_WSI_SWITCH`, `extern WsiBootstrap SwitchWSI` ;
- `src/wsi/switch/wsi_platform_switch.cpp` : extensions `VK_KHR_surface` + `VK_NN_vi_surface`,
  `createSurface(HWND, pfn, instance, pSurface)` → `vkCreateViSurfaceNN` avec le `NWindow*` ;
- `src/wsi/switch/wsi_window_switch.cpp` : taille fixe (1280×720 portable, 1920×1080 docké),
  plein écran, pas de redimensionnement ;
- `src/wsi/switch/wsi_monitor_switch.cpp` : un seul moniteur, modes 720p/1080p ;
- `meson.build` : ajouter les fichiers et `-DDXVK_WSI_SWITCH`.

Le point délicat est le passage du `HWND` abstrait au `NWindow*` : le runtime Win32
(`rt_win.cpp`) crée aujourd'hui une `SDL_Window`. Sur Switch, il faut soit un backend fenêtre
NWindow, soit un handle opaque qui transporte le `NWindow*` jusqu'au WSI. Le plus simple pour le
bring-up est un backend fenêtre dédié, sans SDL.

### Détail mémoire

Le runtime suppose une région contiguë de 4 GiB. Horizon permet de réserver de l'espace d'adresses,
mais pas de le peupler à la demande comme `mmap(PROT_NONE)` + `mprotect`. Trois options, dans l'ordre :

1. **Mesurer** le pic réel du heap invité sous Linux (compteur high-water dans `rt_proc.c`) ;
   CT3 n'utilise probablement pas 960 MiB.
2. **Réduire l'arène** au besoin réel (image ~3.9 Mo + scratch 4 Mo + pile 1 Mo + heap mesuré),
   réserver et committer cette plage au démarrage.
3. Si la réservation contiguë reste impossible, introduire une **table de pages** derrière
   `guest_ld`/`guest_st`/`rt_gptr` : refactor plus lourd, à éviter tant que l'option 2 suffit.

### Mesure du pic de heap (L1.1 / S2.1)

Session de course de 130 s sur display réel, `CT3_WINDOWED=1`, navigation
`CT3_KEYS=1200,1600,2000,2400` : le pic atteint **`0x10A78EB0` = 279 412 400 octets
(~266 Mio)**, après le chargement de la course, puis plafonne.

Session post-course plus longue (300 s, `CT3_KEY_AT2=2800` pour valider AXEL,
`CT3_KEY_HOLD=3600:up`, course complète) : le pic historique monte à
**`0x110B3C70` = 285 737 072 octets (~272,5 Mio)**, avec `ExitProcess(0)` et
code 0 (D-079). Les valeurs de tas observées en fin de session :
sommet `0x10E2C160`, utilisé ~273,5 Mio, plus grand bloc libre ~248,8 Kio.

Session longue L1.3 (18 nov. 2026, `CT3_HEAPSTATS_MS=60000`, navigation/course
automatisée) : pas de faute mémoire, pas d'« tas invité épuisé », exceptions FP
CRT ignorées (D-077), sortie par `ExitProcess(13)` (atexit exécuté). Le tas se
stabilise un temps autour de ~269 Mio puis connaît une croissance tardive ;
**pic historique observé `0x1AB89C00` = 448 173 056 octets (~427,4 Mio)**, utilisé
~447,2 Mio, plus grand bloc libre ~114,9 Kio (fin de log, `/tmp/opencode/ct3_l1_3.log`).
La tendance ascendante en fin de session a ensuite été expliquée par les arènes
`VirtualAlloc` jamais libérées et par le cache de transit D3D9 non borné ; le
correctif et l'A/B sont ci-dessous (D-081).

Contre-mesure (18 nov. 2026, run de 20 min, `CT3_ALLOCSTATS=1`, `VirtualFree`
honoré) : le tas reste **stable à ~160,5 Mio utilisés** (pic `0x0B22D9D0` =
186 767 824 o, ~178 Mio) de t=160 s à t=1199 s, sans faute ni épuisement ;
sortie par SIGTERM (`ExitProcess(0)`, code 0). Le profil par site montre
**84,7 Mio alloués via `VirtualAlloc` et 79,7 Mio effectivement libérés** (le
`VirtualFree` était auparavant un no-op), plus ~71 Mio de zones de transit D3D9
vivantes (cache non encore borné sur ce run).

Run court A/B (360 s, même scénario, `CT3_ALLOCSTATS=1`) : **143,9 Mio utilisés,
pic 161 Mio** avec le correctif, contre **240,4 Mio utilisés, pic 243 Mio** avec
`CT3_VIRTUALFREE_NOOP=1`. Le site `VirtualAlloc` passe de **84,7 Mio alloués /
0 libéré** (80 Mio vivants) à **84,7 Mio alloués / 79,7 Mio libérés** (5 Mio
vivants) : la récupération des arènes est prouvée par l'A/B, à scénario égal.
Le cache de transit D3D9 est en outre borné à 64 Mio (meilleur ajustement, élagage
des plus grosses zones). La croissance à 448 Mio ne s'est pas reproduite sur ces
runs. Le test de changement de ville (West Coast → Glitter Oasis) n'a pas chargé
de seconde ville avec la séquence de touches actuelle : il reste à rendre
déterministe, ou à accepter l'A/B + le soak 60 min comme clôture de L1.3.

L'ancien arrêt à 9 058 présentations était une exception FP du CRT
(`0xC000008F`, `STATUS_FLOAT_INEXACT_RESULT`, voir D-077), pas un épuisement du
tas. Après le contournement, la même session dépasse 17 000 présentations (D-077).

Conséquence pour S2 : le pic stabilisé est de **161 Mio** après le correctif L1.3 (D-081), mais
l'arène est planifiée à **512 Mio** tant que le soak de 60 min n'a pas confirmé l'absence de
croissance sur session longue (G2). Le runtime signale la croissance du pic
(`CT3_HEAP_WARN_MB`, défaut 448, et `CT3_HEAP_WARN_STEP_MB`, défaut 64), ce qui permet un
contournement de suivi sans bloquer le bring-up sur un test de 60 min continu. Prévoir en plus
l'image (~3,9 Mo), le scratch (4 Mo) et les piles de threads.

## 4. Plan par jalons

| Jalon | Contenu | Critère d'acceptation | Ordre de grandeur |
|---|---|---|---|
| M0 | Base Linux figée : deux commits (runtime, portabilité AArch64), tag `v0.1.0-linux`, documents obsolètes archivés sous `docs/archive/`. | **fait** (17 nov. 2026). | 1–2 jours |
| M1 | Spike AArch64 Linux. | `test_x87`, `test_cpu`, `test_vec` OK. | **fait** |
| M2 | Abstraction mémoire + pic de heap + toolchain devkitA64. | `test_g2` sur AArch64 Linux ; sonde `long double` sur devkitA64. | 2–3 semaines |
| M3 | Backend WSI Switch + DXVK loaderless. | Smoke test Vulkan Switch, puis triangle DXVK présenté. | 3–5 semaines |
| M4 | Bring-up CT3 : menus, puis une course. | Menus 2D + une course jouable en 720p, entrée et audio. | 4–8 semaines |
| M5 | Performance et stabilité. | Cadence stable, pas de crash connu, profiling documenté. | continu |

Total indicatif : **3 à 6 mois** à temps partiel, dominé par M3 (WSI) et M5 (performance).
L'incertitude principale reste la toolchain newlib (`long double`) et le comportement de NVK
sur les shaders de CT3.

## 5. Reproduction du spike

Prérequis : un build Linux existant (`test_x87`, `ct3pack`, `test_vec`), `podman`,
binfmt AArch64.

```sh
cmake --build build --target test_x87 test_g2 ct3pack test_vec
tools/port/aarch64_spike.sh
```

Le script monte le dépôt en lecture seule et compile dans `alpine:3.20` arm64 ; il ne modifie rien.
`CT3_BUILD_DIR=/autre/build` sélectionne un autre répertoire de build.

Résultat attendu :

```
F80 CROISE OK (0 echec(s))
X87 OK (0 échec(s))
G2 OK (0 échec(s))
CPU OK (0 échec(s))
VEC OK (0 échec(s))
```

## 6. Décisions à prendre

1. **DXVK Native** (recommandé) plutôt qu'un renderer D3D9 maison : le runtime forwarde déjà 119
   méthodes COM vers DXVK ; le travail restant est le WSI et le loaderless.
2. **Arène contiguë dimensionnée** (recommandé) plutôt qu'une table de pages, si le pic de heap
   mesuré le permet.
3. **`long double`** : tranché par S1. newlib est binary128 little-endian (113 bits,
   exposant 16384) ; le chemin rapide de `rt_fp.h` s'applique, sans type `extFloat80`.
   Conserver les tests x87 comme non-régression.
4. **Fenêtre NWindow dédiée** (recommandé) plutôt que SDL2 Switch pour le premier bring-up.
5. **Cinématiques** : les désactiver au premier bring-up (le chemin FFmpeg est récent et non
   commité), puis les réintroduire après M4.

## 7. Références

- `https://github.com/danfromtico/mesa-switch` — NVK Vulkan, `VK_NN_vi_surface`, WSI NWindow.
- `https://github.com/dantiicu/vulkan-smoke-test-switch` — smoke test instance/device/surface/
  swapchain/present (dépôt archivé, reste une référence utile).
- `~/.local/ct3-deps/src/dxvk` — DXVK 3.1.1, commit `21fda8f`.
- `tools/port/aarch64_spike.sh` — reproduction du spike AArch64.
- `tools/port/f80_crosscheck.c` — validation croisée x87 ↔ binary128 (motifs aléatoires).
- `tools/port/devkita64_probe.sh` — sonde S1 : macros, newlib bare-metal, fonctionnalités libnx.
- `tools/port/newlib_probe.c` — sonde exécutée par `qemu-system-aarch64` (format, `ldexpl`/`frexpl`).
- `tools/port/switch_features.c` — vérification des symboles et du TLS côté Switch.
- `docs/PACK.md` — contrat du pack de données.
- `DECISIONS.md` D-011 — séparation backend/runtime, Switch reporté.
