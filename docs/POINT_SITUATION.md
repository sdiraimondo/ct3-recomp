# Point de situation — ct3-linux

**Date :** 19 novembre 2026\
**Branche :** `master` = `origin/master`\
**Base figée :** tag `v0.1.1-linux` (`7149afb`)\
**Derniers commits :**
- `c6f2d32` — Outillage : architecture orchestrateur/codeur OpenCode
- `a078842` — Docs : roadmap et point de situation au 19 nov. 2026
- `5684648` — Docs : code de sortie = `wParam` du dernier message (D-083)
- `7149afb` — Licence : notice GPL complète dans le README
- `2f211b7` — Publication : GPL-3.0-or-later, notices et build sans données de jeu
- `46116c9` — Runtime : récupération des arènes `VirtualAlloc`, cache de transit borné (L1.3, D-081)

Références : [`docs/ROADMAP.md`](ROADMAP.md), [`docs/SWITCH_PORT.md`](SWITCH_PORT.md), [`docs/HANDOFF.md`](HANDOFF.md), [`DECISIONS.md`](../DECISIONS.md), [`README.md`](../README.md).

## 1. Périmètre et contraintes (rappel)
- Port Linux x86_64 de `CT3.exe` (PE32, Crazy Taxi 3) par recompilation statique, rendu Vulkan/DXVK ; port Switch (NVK `mesa-switch` + DXVK Native, WSI `VK_NN_vi_surface`) en préparation.
- **Objectif** : décompilation complète et propre → exécutable Linux → exécutable Switch. « Complète » signifie **vérification sémantique fonction par fonction** (D-080) ; ce chantier est **hors du chemin critique** des deux exécutables.
- **Contraintes** : ne jamais modifier `CT3.exe`/`CT3.original.exe` ; ne pas versionner les artefacts dérivés du jeu (pack, C généré, `libct3gen.a`, corpus de vérification) ; pack préservant VA/imports/point d’entrée/BSS ; assets lus hors dépôt. Pas de contournement de protection.
- Route graphique Switch confirmée : pas deko3d. `S1/G1` franchies (newlib/devkitA64 binary128 LE).

## 2. Travaux réalisés (cette séquence)

### Décompilation et complétude
- **D-080** : la complétude de la décompilation est définie par la **vérification sémantique fonction par fonction**, et non par la seule traduction. La mention « décompilation suffisante » (introduite sans arbitrage) est retirée de la roadmap et de `SWITCH_PORT.md`.
- **Chantier `V1`–`V4`** (`docs/ROADMAP.md` §6) : harnais différentiel (référence Unicorn x86-32 contre C traduit), triage des ~5 747 fonctions, vérification par famille (atteignables d’abord), rapport `docs/VERIF_SEMANTIQUE.md` et non-régression. **Aucune porte `G*` ne l’attend** ; il avance en parallèle de `L*`/`S*`.
- Point de départ existant : `tools/ref/diff_run.py` (référence Unicorn, comparaison des séquences d’imports) à étendre en comparaison par fonction ; `tools/ref/gen_vectors.py` pour les cas.

### Runtime / stabilité
- **D-081 (L1.3)** : `VirtualFree(MEM_RELEASE)` libère désormais le bloc du tas invité, avec suivi des adresses rendues par `VirtualAlloc` (pas de double libération ni de bloc étranger) ; `MEM_DECOMMIT` reste sans effet. `CT3_VIRTUALFREE_NOOP=1` reproduit l’ancien no-op pour l’A/B.
- **D-081 (L1.3)** : cache de zones de transit D3D9 borné à **64 Mio** (meilleur ajustement sans limite de taille, élagage des plus grosses zones) ; sans plafond, les tailles variées accumulaient des tampons toute la session.
- **Instrumentation** : `CT3_ALLOCSTATS=1` profile les blocs **vivants** par site appelant (adresse invitée, ou site hôte `0x8…`), dump à chaque rapport de tas et à la sortie ; rapport de tas horodaté (`t=Ns`). Rappel : `heap_peak_top` monotone, `CT3_HEAPSTATS`, `CT3_HEAP_WARN_MB` (448) / `CT3_HEAP_WARN_STEP_MB` (64).
- **D-077 (L1.2)** : exceptions FP du CRT `0xC000008D`–`0xC0000093` ignorées (retour au CRT qui remet les drapeaux FP), les autres restent fatales.
- **D-078 (L2.1)** : sortie CRT sans SEH — `atexit` natif `0x51549F/0x5154B5`, nettoyages `0x514919`/`0x5164E5`/`0x5206C7`, `rt_unresolved_ret` dépile l’adresse de retour.
- **D-079** : `IDirectSoundBuffer::Release` retire le tampon du mixeur SDL sous `ds_mutex`, bornage du stockage au tas invité, `ds_audio_stop()` avant `exit()`.
- **D-083 (L2.2)** : le code de sortie du jeu est le `wParam` du dernier message dépilé (`WinMain` 0x004079E0 → boucle 0x00428070 → `MOV EAX,[ESP+0xC]` à 0x00428212), pas un statut : 13 = `VK_RETURN`, 88 = `VK_X` (bouton A mappé sur `x`). Critère de propreté = `atexit`, fichiers fermés, aucune faute. L2/L2.2 closes.

### Port Switch (préparation)
- **S4.1 (close, D-084)** : option Meson `native_switch` + `-DDXVK_WSI_SWITCH`, bootstrap WSI Switch, `src/wsi/switch/*` (surface `VK_NN_vi_surface` via `vkCreateViSurfaceNN`, modes 720p/1080p, stubs fenêtre/moniteur, convention `NWindow*` à stabiliser).
  - **Cross-build validé** (19 nov. 2026) en conteneur `localhost/devkitpro-mesa-rust:latest` (Debian 12, meson 1.12.1, devkitA64 15.2.0, libnx) : `native_switch : true`, `-DDXVK_WSI_SWITCH: YES`, 3/3 objets `switch_wsi_*` compilés (58 symboles `SwitchWsi`).
  - Correctifs apportés : conflit de macro `interface` (en-têtes libnx contre `windows_base.h`), types `VK_NN_vi_surface` déclarés localement (`wsi_switch_vulkan_compat.h`, garde `#ifndef`), `kMonitor` en `static inline const`, et **WSI Switch exclusif** (SDL2/SDL3/GLFW désactivés en mode Switch).
  - Patch reproductible dans le dépôt : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch` (11 fichiers, source de vérité, appliqué sur DXVK 3.1.1 `21fda8f`, `git apply --check` OK).
  - Reproduction : `tools/port/dxvk_switch_wsi_build.sh` (`RESULT=PASS`).
  - **Reste bloqué (hors S4.1, prérequis S7)** : l'archive `src/wsi/libwsi.a` échoue sur `wsi_edid.cpp` → `src/util/thread.h:375` (`::pthread_setschedparam` absent sur Horizon/newlib) ; mode strict `CT3_DXVK_REQUIRE_LIBWSI=1` pour l'exiger après S7.

### Documentation
- `docs/ROADMAP.md` : objectif de complétude, chantier `V1`–`V4` hors chemin critique, diagramme et Gantt mis à jour, mesures L1.3/S2.1, matrice et risques.
- `docs/SWITCH_PORT.md` : verdict corrigé (le port réutilise le code décompilé et ne clôt pas la décompilation), mesures A/B.
- `DECISIONS.md` : D-011 corrigée, D-080 (complétude), D-081 (tas).
- `docs/HANDOFF.md`, `README.md` : variables `CT3_ALLOCSTATS`, `CT3_VIRTUALFREE_NOOP`, état courant.
- **Licence et publication (D-082)** : code original sous GPL-3.0-or-later ; publication **source seule** ; `LICENSE`, `THIRD_PARTY.md`, `LICENSES/`, `LEGAL.md`, `CONTRIBUTING.md`, `PUBLISHING.md`, garde-fou `tools/check_no_game_data.sh` et CI. Le bundle `dist/` reste `local-only` car il lie le code traduit du jeu.

## 3. Mesures et validations

### Build & tests
- `cmake --build build` OK ; `test_cpu`, `test_vec`, `test_x87`, `test_g2` verts.

### Tas / sessions
| Scénario | Pic observé | Notes |
|---|---:|---|
| Course 130 s (display réel, `CT3_WINDOWED`, `CT3_KEYS=1200,1600,2000,2400`) | `0x10A78EB0` = 279 412 400 o (~266 Mio) | plateau après chargement |
| Écran AXEL (exception FP à 9058/9059) | `0x0E4799D0` = 239 570 384 o (~228,5 Mio) | pas d’épuisement |
| Post-course 300 s (AXEL validé `CT3_KEY_AT2=2800`, accélération) | `0x110B3C70` = 285 737 072 o (~272,5 Mio) | `ExitProcess(0)`, code 0 (D-079) |
| Run long L1.3 avant correctif (`CT3_HEAPSTATS_MS=60000`, `/tmp/opencode/ct3_l1_3.log`) | **`0x1AB89C00` = 448 173 056 o (~427,4 Mio)** | croissance tardive, `ExitProcess(13)` propre, pas de faute/épuisement |
| Run 20 min avec correctif (`/tmp/opencode/ct3_l1_3b.log`) | `0x0B22D9D0` = 186 767 824 o (~178 Mio) | **stable à ~160,5 Mio utilisés** de t=160 s à t=1199 s, `ExitProcess(0)` |
| Run 360 s avec correctif (`/tmp/opencode/ct3_city_fix2.log`) | `0x0A15D100` = 169 074 944 o (~161 Mio) | **143,9 Mio utilisés**, site `VirtualAlloc` 84,7 Mio alloués / **79,7 Mio libérés** |
| Témoin A/B 360 s `CT3_VIRTUALFREE_NOOP=1` (`/tmp/opencode/ct3_city_fix2_noop.log`) | ~243 Mio | **240,4 Mio utilisés**, site `VirtualAlloc` 84,7 Mio alloués / **0 libéré** (80 Mio vivants) |

**Conclusion mesure** : l’A/B prouve que les arènes `VirtualAlloc` sont récupérées (79,7 Mio libérés contre 0) et que le plateau redescend à ~144–160 Mio ; la croissance à 448 Mio ne s’est pas reproduite. Le test de changement de ville (West Coast → Glitter Oasis) n’a **pas** chargé de seconde ville avec la séquence de touches actuelle : il faut le rendre déterministe, ou accepter l’A/B + le soak 60 min comme clôture de L1.3.

### Exceptions FP (D-077)
- Reproduction sur écran AXEL : exception à la présentation 9059, ignorée ; session poursuivie > 17 000 présentations puis sortie propre.

### Sortie (D-078, D-079, L2)
| Chemin | Résultat |
|---|---|
| Fermeture fenêtre / SDL_QUIT / SIGTERM (dont en course) | `atexit` 8→`0x514A27`, 9→`0x5149AA`, `ExitProcess(0)`, code 0 |
| Menu EXIT (séquence automatisée `CT3_KEY_SEQ`) | `atexit` OK, `ExitProcess(13)`, code 13, teardown propre (`/tmp/opencode/ct3_menu_exit.log`) ; 13 = `VK_RETURN` du dernier message |
| Menu EXIT (session utilisateur, 19 nov. 2026) | `atexit` OK, `ExitProcess(88)`, code 88, teardown propre (`/tmp/opencode/ct3-play.log`) ; 88 = `VK_X` (bouton A de la manette ou touche `x`) |

Le code de sortie n'est pas un statut du jeu : `WinMain` retourne le `wParam` du dernier message
dépilé (D-083). Le critère de propreté est l'exécution des `atexit`, la fermeture des fichiers et
l'absence de faute.

### Vérification sémantique (V)
- Aucune mesure à ce jour : chantier non commencé, volontairement hors chemin critique.

## 4. Décisions structurantes récentes
- **D-080** : complétude = vérification sémantique fonction par fonction ; hors chemin critique Linux/Switch.
- **D-081** : `VirtualFree(MEM_RELEASE)` honoré + cache de transit D3D9 borné à 64 Mio ; A/B à l’appui.
- **Contournement L1.3/S2** (maintenu) : arène Switch planifiée à **512 Mio** + alertes `CT3_HEAP_WARN_*` tant que le soak 60 min n’est pas clos ; non bloquant pour S2.

## 5. État de la roadmap (synthèse)
- ✅ **Fait** : M0, M1, S1/G1, L1.1, L1.2, L1.3 (correctif), L2.1, L2.2, S2.1 (pic stabilisé), S4.1 (squelette WSI Switch compilé en cross, D-084).
- 🔄 **En cours** : L1.3 (soak 60 min = clôture), L3 (cinématiques).
- ⬜ **À venir (priorités)** : L1.3 clôture, S4.2/S4.3 (surface `NWindow*`, fenêtre sans SDL) puis S4.4, S2.2 abstraction mémoire invitée, S3+ selon chemin critique `S1→S2→S4→S5→S8→S9→S10`.
- ⬜ **Vérification sémantique `V1`–`V4`** : en parallèle, hors chemin critique ; aucune porte `G*` ne l’attend.

## 6. Prochaines actions recommandées
1. **L1.3 — clôture** : soak de 60 min avec le correctif (`CT3_HEAPSTATS=1`, `CT3_ALLOCSTATS=1`) lors d’une **session de jeu réelle** (plusieurs courses, si possible changement de ville) ; consigner plateau, pic et `ExitProcess`. Reporté à une session de jeu réelle : l’automatisation actuelle ne boucle pas les menus de façon déterministe. Non bloquant pour S2.
2. **Changement de ville** : rendre la séquence déterministe (`Down` + `Enter` sur GLITTER OASIS) ou acter l’A/B comme preuve de récupération.
3. **L2.2 — close (D-083)** : le code de sortie reflète le dernier message (13 clavier, 88 manette) ; le teardown est propre dans les deux chemins.
4. **S4.1 — close (D-084)** : cross-build validé en conteneur (`tools/port/dxvk_switch_wsi_build.sh`, `RESULT=PASS`) ; reste `libwsi.a` bloquée par `src/util/thread.h` (`pthread_setschedparam`), prérequis S7.
5. **S4.2/S4.3** : stabiliser le transport `NWindow*` et `createSurface` réel (`vkCreateViSurfaceNN`), fenêtre plein écran sans SDL, puis viser le smoke test S4.4 (porte G3).
6. **S2.2** : introduire `rt_va_reserve/commit/protect` (Linux inchangé) en préparation de l’arène Horizon (512 Mio de cible de planification).
7. **V1** : démarrer le harnais différentiel en parallèle (`diff_func` sur 10 fonctions témoins, replay des imports), puis `V2`.

## 7. Risques et points d’attention
- **Croissance du tas** : réduite et expliquée par l’A/B, mais la session de 60 min reste à faire ; impact S2/G2 limité par la marge 512 Mio.
- **Écarts sémantiques silencieux** : la traduction n’est pas une preuve ; le chantier `V*` les couvre, sans bloquer les exécutables.
- **Sortie CRT** : `_flushall`/`_fcloseall` no-op — revoir si une sauvegarde/écriture se révèle tronquée.
- **SEH non implémenté** : seules les exceptions FP ciblées sont contournées ; C++ (`0xE06D7363`) et violations d’accès restent fatales.
- **Validation matérielle Switch absente** ; devkitA64/libnx ne sont pas installés sur l’hôte : le cross-build S4.1 passe par le conteneur `localhost/devkitpro-mesa-rust:latest` (meson + devkitA64 + libnx), source montée en lecture seule.
- **Perf/présentation** : Xvfb+lavapipe inadapté aux runs longs ; `xdotool` inefficace sous XWayland (utiliser `CT3_KEY_*`/`CT3_PAD_*`).

## 8. Artefacts et logs utiles
- Logs : `/tmp/opencode/ct3_city_fix2.log`, `/tmp/opencode/ct3_city_fix2_noop.log`, `/tmp/opencode/ct3_l1_3b.log`, `/tmp/opencode/ct3_l1_3.log`, `/tmp/opencode/ct3_menu_exit.log`, `/tmp/opencode/ct3_exit4.log`.
- Patch DXVK S4.1 : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch`.
- Cross-build WSI Switch : `tools/port/dxvk_switch_wsi_build.sh` ; journaux sous `/tmp/opencode/ct3-dxvk-switch-build-*` (`container.log`).
- DXVK local modifié : `~/.local/ct3-deps/src/dxvk` (l’arbre de validation vit dans `/tmp/opencode/s41/dxvk-src`, non versionné).
