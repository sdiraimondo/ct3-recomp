# DECISIONS.md : ct3-linux

Journal des décisions du portage. Une entrée par décision, avec la date et la source.

## D-001 : projet séparé de ct3recomp (30 oct. 2026)

- Décision : `ct3-linux` est un projet distinct de `Dev/ct3recomp`, qui cible le XBE Xbox.
- Source : réponse de l'utilisateur.

## D-002 : binaire de référence CT3.exe (30 oct. 2026)

- Décision : le comportement porté est celui de `CT3.exe` (16:9, 1280×720 par défaut, FOV 80 probable).
- `CT3.original.exe` sert de témoin pour les 4 zones modifiées : `FUN_004079E0` (0x407A89), `FUN_00407D00` (0x407E4B), `FUN_004316A0` (0x43176A), `FUN_0046A1F0` (0x46ADC9). Ces points doivent être paramétrables.
- Source : réponse de l'utilisateur.

## D-003 : stratégie préliminaire option B (30 oct. 2026)

- Décision : recompilation statique x86 → C, avec couche Win32, remplacement ciblé en C natif si nécessaire.
- Statut : préliminaire, à confirmer après la phase 1.
- Source : réponse de l'utilisateur.

## D-004 : vidéo en priorité basse (30 oct. 2026)

- Décision : la lecture vidéo n'est pas prioritaire. Seul `avi/loading.avi` est présent dans la copie.
- Source : réponse de l'utilisateur.

## D-005 : pas d'assets manquants disponibles (30 oct. 2026)

- Décision : `Media/ADX` (vide) et les vidéos manquantes ne sont pas fournis. Les tests se font sans eux.
- Source : réponse de l'utilisateur.

## D-006 : contraintes techniques issues de la phase 0 (30 oct. 2026)

- Pas de relocations : chargement à 0x400000 exactement.
- Pas de TLS ni de répertoire d'exceptions : SEH manuel via `fs:[0]`.
- Texte GDI en `Arial`.
- Source : `Dev/ct3/analysis/faisabilite.md`.

## D-007 : rendu via dxvk-native, validé (30 oct. 2026)

- Décision : rendu D3D9 par dxvk-native. Couche Vulkan maison écartée, sauf échec de dxvk-native (gate G1).
- Source : réponse de l'utilisateur (point 2).

## D-008 : audio via SDL audio, validé (30 oct. 2026)

- Décision : DirectSound remplacé par SDL audio. OpenAL écarté, sauf si les buffers 3D DirectSound sont utilisés (à vérifier).
- Source : réponse de l'utilisateur (point 3).

## D-009 : gates G1, G2, G3 validées (30 oct. 2026)

- G1 : triangle D3D9 via dxvk-native + SDL2 sur la machine cible.
- G2 : chargement de `CT3.exe` à 0x400000 et exécution d'une routine d'initialisation.
- G3 : traduction de toutes les fonctions atteignables, avec rapport.
- Source : réponse de l'utilisateur (point 5).

## D-010 : machine cible (30 oct. 2026)

- Machine de développement et de test : AMD BC-250 (Cyan Skillfish, `1002:13fe`).
- Pilote : RADV GFX1013, Mesa 26.2.4, Vulkan 1.4.354 (vérifié par `vulkaninfo --summary`). Vulkan 1.3 requis par dxvk-native : condition remplie.
- Source : réponse de l'utilisateur (point 6), vérification sur la machine.

## D-011 : portage Switch reporté (30 oct. 2026)

- Décision : le portage Switch est une étape ultérieure. Le portage Linux PC est traité en premier.
- Conséquence d'architecture à respecter dès maintenant : la couche D3D9 et le backend de rendu sont séparés, pour qu'un backend autre que Vulkan (Switch) puisse remplacer dxvk-native plus tard. Le code généré reste du C portable, sans assembleur en ligne. La couche plateforme passe par une interface.
- Source : réponse de l'utilisateur (point 6).

## D-012 : xboxrecomp non retenu comme base du traducteur (30 oct. 2026)

- Décision : pas de base `xboxrecomp` (projet en développement, cible Xbox). Le traducteur est écrit pour ce projet. Choix technique (Ghidra p-code ou capstone) à trancher en phase 2.
- Source : réponse de l'utilisateur (point 4).

## D-013 : dxvk-native compilé localement, gate G1 franchie (30 oct. 2026)

- Source : `github.com/doitsujin/dxvk`, branche `master`, commit `21fda8f` (version 3.1.1), sous-modules en superficiel (Vulkan-Headers, SPIRV-Headers, dxbc-spirv, libdisplay-info, mingw-directx-headers).
- Build : meson 1.12.1 (venv `~/.venvs/ct3recomp`), `--buildtype release`, `-Dnative_sdl2=enabled`, SDL3 et GLFW désactivés, 64 bits uniquement. Installé dans `~/.local/ct3-deps/prefix` (lib `lib64/libdxvk_d3d9.so`, en-têtes `include/dxvk`).
- SDL2 2.32.10 réutilisé depuis `~/.local/ct3-deps/prefix` (déjà compilé pour `ct3recomp`).
- Test : `tests/g1_d3d9_triangle` (triangle D3D9, relecture d'un pixel). Résultat : `pixel(320,300) = 0xff505858`, `G1 OK`, sur AMD BC-250 (RADV GFX1013, Mesa 26.2.4). La valeur correspond à l'interpolation attendue (80, 88, 88).
- Session : Wayland avec XWayland (DISPLAY=:0). SDL2 utilise X11.
- Non traité : `DXVK_WSI_DRIVER=SDL2` est positionné dans le code du test. Les avertissements du chargeur Vulkan (ICD `dzn`, couche `LS_frame_generation`) sont sans effet sur le résultat.

## Points ouverts

- Traducteur : source sémantique à choisir. Proposition : export p-code via Ghidra headless (Ghidra 12.1.4 déjà installé). Alternative : capstone et traducteur maison.
- Démarrage de la phase 2 : en attente de confirmation.
