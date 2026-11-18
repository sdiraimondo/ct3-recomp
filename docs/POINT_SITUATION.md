# Point de situation — ct3-linux

**Date :** 18 novembre 2026  
**Branche :** `master` = `origin/master`  
**Base figée :** tag `v0.1.0-linux` (`0d5dff7`)  
**Derniers commits :**
- `46116c9` — Runtime : récupération des arènes `VirtualAlloc`, cache de transit borné (L1.3, D-081)
- `dbe6137` — Docs : point de situation (18 nov. 2026)
- `5946d9a` — Runtime/docs : alertes de pic de tas et contournement L1.3/S2 (marge 512 Mio)
- `61df87e` — Docs : résultats L1.3 (pic 448 Mio, croissance tardive) et priorités

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

### Port Switch (préparation)
- **S4.1 (squelette, en cours)** : option Meson `native_switch` + `-DDXVK_WSI_SWITCH`, bootstrap WSI Switch, `src/wsi/switch/*` (surface `VK_NN_vi_surface` via `vkCreateViSurfaceNN`, modes 720p/1080p, stubs fenêtre/moniteur, convention `NWindow*` à stabiliser).
  - Patch reproductible dans le dépôt : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch` (source de vérité, à appliquer sur DXVK 3.1.1 local).
  - Arbre DXVK local modifié : `~/.local/ct3-deps/src/dxvk` (non commité dans ce dépôt externe).
  - **Build cross devkitA64/libnx non validé sur l’hôte actuel**.

### Documentation
- `docs/ROADMAP.md` : objectif de complétude, chantier `V1`–`V4` hors chemin critique, diagramme et Gantt mis à jour, mesures L1.3/S2.1, matrice et risques.
- `docs/SWITCH_PORT.md` : verdict corrigé (le port réutilise le code décompilé et ne clôt pas la décompilation), mesures A/B.
- `DECISIONS.md` : D-011 corrigée, D-080 (complétude), D-081 (tas).
- `docs/HANDOFF.md`, `README.md` : variables `CT3_ALLOCSTATS`, `CT3_VIRTUALFREE_NOOP`, état courant.

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
| Menu EXIT (séquence automatisée `CT3_KEY_SEQ`) | `atexit` OK, `ExitProcess(13)`, code 13, teardown propre (`/tmp/opencode/ct3_menu_exit.log`) |

### Vérification sémantique (V)
- Aucune mesure à ce jour : chantier non commencé, volontairement hors chemin critique.

## 4. Décisions structurantes récentes
- **D-080** : complétude = vérification sémantique fonction par fonction ; hors chemin critique Linux/Switch.
- **D-081** : `VirtualFree(MEM_RELEASE)` honoré + cache de transit D3D9 borné à 64 Mio ; A/B à l’appui.
- **Contournement L1.3/S2** (maintenu) : arène Switch planifiée à **512 Mio** + alertes `CT3_HEAP_WARN_*` tant que le soak 60 min n’est pas clos ; non bloquant pour S2.

## 5. État de la roadmap (synthèse)
- ✅ **Fait** : M0, M1, S1/G1, L1.1, L1.2, L1.3 (correctif), L2.1, S2.1 (pic stabilisé).
- 🔄 **En cours** : L1.3 (soak 60 min = clôture), L2/L2.2 (teardown propre, code 13 menu EXIT à confirmer), L3 (cinématiques), S4.1 (squelette/patch, build cross à valider).
- ⬜ **À venir (priorités)** : L1.3 clôture, L2.2, S4.1 build devkitA64/libnx puis S4.2/S4.3, S2.2 abstraction mémoire invitée, S3+ selon chemin critique `S1→S2→S4→S5→S8→S9→S10`.
- ⬜ **Vérification sémantique `V1`–`V4`** : en parallèle, hors chemin critique ; aucune porte `G*` ne l’attend.

## 6. Prochaines actions recommandées
1. **L1.3 — clôture** : soak de 60 min avec le correctif (`CT3_HEAPSTATS=1`, `CT3_ALLOCSTATS=1`) ; consigner plateau, pic et `ExitProcess`. Non bloquant pour S2.
2. **Changement de ville** : rendre la séquence déterministe (`Down` + `Enter` sur GLITTER OASIS) ou acter l’A/B comme preuve de récupération.
3. **L2.2** : confirmer si `ExitProcess(13)` est le code nominal du menu EXIT ; ajuster la séquence si besoin ; clore L2 si les chemins jeu sont propres.
4. **S4.1** : valider la compilation `-Dnative_switch=true` du patch DXVK en cross devkitA64/libnx ; corriger includes/dépendances.
5. **S4.2/S4.3** : stabiliser le transport `NWindow*` et `createSurface`/fenêtre plein écran.
6. **S2.2** : introduire `rt_va_reserve/commit/protect` (Linux inchangé) en préparation de l’arène Horizon (512 Mio de cible de planification).
7. **V1** : démarrer le harnais différentiel en parallèle (`diff_func` sur 10 fonctions témoins, replay des imports), puis `V2`.

## 7. Risques et points d’attention
- **Croissance du tas** : réduite et expliquée par l’A/B, mais la session de 60 min reste à faire ; impact S2/G2 limité par la marge 512 Mio.
- **Écarts sémantiques silencieux** : la traduction n’est pas une preuve ; le chantier `V*` les couvre, sans bloquer les exécutables.
- **Sortie CRT** : `_flushall`/`_fcloseall` no-op — revoir si une sauvegarde/écriture se révèle tronquée.
- **SEH non implémenté** : seules les exceptions FP ciblées sont contournées ; C++ (`0xE06D7363`) et violations d’accès restent fatales.
- **Validation matérielle Switch absente** ; devkitA64/libnx non disponibles sur l’hôte (build cross S4.1 à faire ailleurs/conteneur dédié).
- **Perf/présentation** : Xvfb+lavapipe inadapté aux runs longs ; `xdotool` inefficace sous XWayland (utiliser `CT3_KEY_*`/`CT3_PAD_*`).

## 8. Artefacts et logs utiles
- Logs : `/tmp/opencode/ct3_city_fix2.log`, `/tmp/opencode/ct3_city_fix2_noop.log`, `/tmp/opencode/ct3_l1_3b.log`, `/tmp/opencode/ct3_l1_3.log`, `/tmp/opencode/ct3_menu_exit.log`, `/tmp/opencode/ct3_exit4.log`.
- Patch DXVK S4.1 : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch`.
- DXVK local modifié : `~/.local/ct3-deps/src/dxvk`.
