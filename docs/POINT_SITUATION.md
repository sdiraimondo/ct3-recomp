# Point de situation — ct3-linux

**Date :** 18 novembre 2026  
**Branche :** `master` = `origin/master` (`5946d9a`)  
**Base figée :** tag `v0.1.0-linux` (`0d5dff7`)  
**Derniers commits :**
- `30be5a8` — Runtime : stabilité L1/L2 et outillage de validation
- `487d74f` — Docs : pic de heap post-course (L1.3/S2) et S4.1 en cours
- `84713f7` — S4.1 : squelette WSI Switch DXVK (patch) et validation L2.2
- `61df87e` — Docs : résultats L1.3 (pic 448 Mio, croissance tardive) et priorités
- `5946d9a` — Runtime/docs : alertes de pic de tas et contournement L1.3/S2 (marge 512 Mio)

Références : [`docs/ROADMAP.md`](ROADMAP.md), [`docs/SWITCH_PORT.md`](SWITCH_PORT.md), [`docs/HANDOFF.md`](HANDOFF.md), [`DECISIONS.md`](../DECISIONS.md), [`README.md`](../README.md).

## 1. Périmètre et contraintes (rappel)
- Port Linux x86_64 de `CT3.exe` (PE32, Crazy Taxi 3) par recompilation statique, rendu Vulkan/DXVK ; port Switch (NVK `mesa-switch` + DXVK Native, WSI `VK_NN_vi_surface`) en préparation.
- **Contraintes** : ne jamais modifier `CT3.exe`/`CT3.original.exe` ; ne pas versionner les artefacts dérivés du jeu (pack, C généré, `libct3gen.a`) ; pack préservant VA/imports/point d’entrée/BSS ; assets lus hors dépôt. Pas de contournement de protection.
- Route graphique Switch confirmée : pas deko3d. `S1/G1` franchies (newlib/devkitA64 binary128 LE).

## 2. Travaux réalisés (cette séquence)

### Runtime / stabilité
- **D-077 (L1.2)** : `RaiseException` (KERNEL32 `0x53b21c`) — les exceptions FP du CRT `0xC000008D`–`0xC0000093` (observé `0xC000008F` `STATUS_FLOAT_INEXACT_RESULT` à `0x523B9C`/rendu) sont ignorées (retour au CRT qui remet les drapeaux FP), les autres restent fatales. Pas d’implémentation SEH.
- **D-078 (L2.1)** : sortie CRT sans SEH — lanceur `atexit` MSVC `0x51549F/0x5154B5` implémenté nativement, nettoyages `0x514919` (`_mtdeletelocks`, no-op), `0x5164E5` (`_flushall`, 0), `0x5206C7` (`_fcloseall`, 0) ; `rt_unresolved_ret` dépile l’adresse de retour comme le `RET` traduit (registre 644/EIP). Note : fichiers CRT encore ouverts non vidés (le jeu ferme les siens).
- **D-079** : `IDirectSoundBuffer::Release` retire le tampon du mixeur SDL sous `ds_mutex`, bornage du stockage au tas invité (`RT_HEAP_BASE`..`RT_HEAP_BASE+RT_HEAP_SIZE`), `ds_audio_stop()` avant `exit()` dans `ExitProcess` — corrige la faute du fil audio post-course (`0x0DC50034`).
- **Instrumentation** : pic de tas monotone `heap_peak_top` (non réduit par `rt_heap_set_state`), `CT3_HEAPSTATS`/`CT3_HEAPSTATS_MS` (rapport avec pic en octets/Mio), rapport forcé à l’épuisement ; alertes de croissance `CT3_HEAP_WARN_MB` (448) / `CT3_HEAP_WARN_STEP_MB` (64).
- **Robustesse** : handler de crash borné à `t->stack_low`/`t->stack_top` (évite seconde faute et permet core exploitable).
- **Outils de validation** : `CT3_WINDOWED`, `CT3_PAD_HOLD`, `CT3_KEY_HOLD`, `CT3_KEY_SEQ`, `CT3_ATEXIT_LOG`, diagnostics `_onexit` (`0x7DD0EC/0x7DD0E8`).

### Port Switch (préparation)
- **S4.1 (squelette, en cours)** : option Meson `native_switch` + `-DDXVK_WSI_SWITCH`, bootstrap WSI Switch, `src/wsi/switch/*` (surface `VK_NN_vi_surface` via `vkCreateViSurfaceNN`, modes 720p/1080p, stubs fenêtre/moniteur, convention `NWindow*` à stabiliser).
  - Patch reproductible dans le dépôt : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch` (à appliquer sur DXVK 3.1.1 local).
  - Arbre DXVK local modifié : `~/.local/ct3-deps/src/dxvk` (non commité dans ce dépôt externe).
  - **Build cross devkitA64/libnx non validé sur l’hôte actuel**.

### Documentation
- `docs/ROADMAP.md`, `docs/SWITCH_PORT.md`, `docs/HANDOFF.md`, `DECISIONS.md` (D-077..D-079), `README.md` mis à jour (mesures, états L1/L2/S2/S4.1, variables de diagnostic, contournement L1.3/S2).

## 3. Mesures et validations

### Build & tests
- `cmake --build build` OK ; `test_cpu`, `test_vec`, `test_x87`, `test_g2` verts.

### Tas / sessions
| Scénario | Pic observé | Notes |
|---|---:|---|
| Course 130 s (display réel, `CT3_WINDOWED`, `CT3_KEYS=1200,1600,2000,2400`) | `0x10A78EB0` = 279 412 400 o (~266 Mio) | plateau après chargement |
| Écran AXEL (ancienne session, exception FP à 9058/9059) | `0x0E4799D0` = 239 570 384 o (~228,5 Mio) | pas d’épuisement |
| Post-course 300 s (AXEL validé `CT3_KEY_AT2=2800`, accélération) | `0x110B3C70` = 285 737 072 o (~272,5 Mio) | `ExitProcess(0)`, code 0 (D-079) |
| Run long L1.3 (18 nov., `CT3_HEAPSTATS_MS=60000`, `/tmp/opencode/ct3_l1_3.log`) | **`0x1AB89C00` = 448 173 056 o (~427,4 Mio)** | stabilisation ~269 Mio puis **croissance tardive** (337→448 Mio) en fin de session, `ExitProcess(13)` propre, pas de faute/épuisement, 4 FP ignorées |

**Conclusion mesure** : absence d’épuisement/fautes sur les runs réalisés, mais **croissance tardive du tas** (fuite/fragmentation ou transition) à investiguer ; pic de planification S2 revu à la hausse.

### Exceptions FP (D-077)
- Reproduction sur écran AXEL : exception à la présentation 9059, ignorée ; session poursuivie > 17 000 présentations puis sortie propre.

### Sortie (D-078, D-079, L2)
| Chemin | Résultat |
|---|---|
| Fermeture fenêtre / SDL_QUIT / SIGTERM (dont en course) | `atexit` 8→`0x514A27`, 9→`0x5149AA`, `ExitProcess(0)`, code 0 |
| Menu EXIT (séquence automatisée `CT3_KEY_SEQ`) | `atexit` OK, `ExitProcess(13)`, code 13, teardown propre (`/tmp/opencode/ct3_menu_exit.log`) |

## 4. Contournement L1.3 / S2 (adopté pour ne pas bloquer le port)
- **Dimensionnement** : base prudente ≥ ~450 Mio observés, **marge de planification 512 Mio** pour l’arène Switch (G2) + image/scratch/piles.
- **Suivi** : `CT3_HEAPSTATS` (pic en Mio) et alertes `CT3_HEAP_WARN_MB/STEP` pour détecter la dérive sans soak 60 min continu.
- **Démarche** : ne pas bloquer S2/bring-up sur le 60 min ; investiguer la croissance tardive sur scénarios courts/transitionnels, corriger, re-mesurer un pic stabilisé ; conserver le 60 min continu comme clôture finale de L1.3.

## 5. État de la roadmap (synthèse)
- ✅ **Fait** : M0, M1, S1/G1, L1.1, L1.2, S2.1 (mesures acquises), L2.1.
- 🔄 **En cours** : L1.3 (croissance tardive à investiguer, critère 60 min non clos), L2/L2.2 (teardown propre, code 13 menu EXIT à confirmer), L3 (cinématiques), S4.1 (squelette/patch, build cross à valider).
- ⬜ **À venir (priorités)** : L1.3 (cause/correction/pic stabilisé), L2.2 clôture, S4.1 build devkitA64/libnx puis S4.2/S4.3, S2.2 abstraction mémoire invitée, S3+ selon chemin critique `S1→S2→S4→S5→S8→S9→S10`.

## 6. Prochaines actions recommandées
1. **L1.3** : reproduire les transitions de fin de course/menu en boucle courte avec `CT3_ALLOCLOG`, `CT3_HEAPCHK`, `CT3_HEAPSTATS` ; identifier fuite vs allocations légitimes ; corriger puis mesurer le plateau/pic.
2. **L2.2** : confirmer si `ExitProcess(13)` est le code nominal du menu EXIT ; ajuster la séquence si besoin ; marquer L2 clos si les chemins jeu sont propres.
3. **S4.1** : valider la compilation `-Dnative_switch=true` du patch DXVK en cross devkitA64/libnx ; corriger includes/dépendances.
4. **S4.2/S4.3** : stabiliser le transport `NWindow*` (handle/runtime) et `createSurface`/fenêtre plein écran.
5. **S2.2** : introduire `rt_va_reserve/commit/protect` (Linux inchangé) en préparation de l’arène Horizon (512 Mio de cible de planification).
6. **Reproductibilité DXVK** : conserver/maintenir `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch` comme source de vérité pour S4.

## 7. Risques et points d’attention
- **Croissance du tas** : peut dépasser les marges sur sessions très longues si non corrigée (impact S2/G2).
- **Sortie CRT** : `_flushall`/`_fcloseall` no-op — revoir si une sauvegarde/écriture se révèle tronquée.
- **SEH non implémenté** : seules les exceptions FP ciblées sont contournées ; C++ (`0xE06D7363`) et violations d’accès restent fatales.
- **Validation matérielle Switch absente** ; devkitA64/libnx non disponibles sur l’hôte (build cross S4.1 à faire ailleurs/conteneur dédié).
- **Perf/présentation** : Xvfb+lavapipe inadapté aux runs longs ; `xdotool` inefficace sous XWayland (utiliser `CT3_KEY_*`/`CT3_PAD_*`).

## 8. Artefacts et logs utiles
- Logs : `/tmp/opencode/ct3_l1_3.log`, `/tmp/opencode/ct3_menu_exit.log`, `/tmp/opencode/ct3_race2.log`, `/tmp/opencode/ct3_fp2.log`, `/tmp/opencode/ct3_exit4.log`.
- Patch DXVK S4.1 : `tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch`.
- DXVK local modifié : `~/.local/ct3-deps/src/dxvk`.
