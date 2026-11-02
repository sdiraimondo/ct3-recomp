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

## D-014 : traducteur fondé sur le p-code Ghidra (provisoire, 30 oct. 2026)

- Décision : le traducteur lit le p-code exporté par Ghidra headless (`tools/ghidra/ExportFunctions.java`, mode `pcode`) et produit du C (`tools/recomp/ct3rc.py`).
- Raison : la sémantique des drapeaux, des chaînes (`rep`, `repne scasb`) et des sous-registres est fixée par Ghidra (SLEIGH). Le traducteur n'a pas à la réinventer. Trois routines (`_strrchr`, `FUN_0042E740`, `FUN_004C1960`) traduites sans aucune opération non prise en charge.
- Non fait : la comparaison avec une approche capstone promise en fin de gate G1. Je ne l'ai pas réalisée. Le choix est donc fondé sur la sémantique fournie par Ghidra, pas sur une mesure comparée. Reste révisable en G3 si le p-code produit du C trop lourd ou trop lent.
- Limites connues : les opérations flottantes (x87, SSE) ne sont pas encore prises en charge (signalées par `rt_unsupported`). `CALLOTHER` (par exemple `rdtsc` dans `FUN_00407EC0`) n'est pas non plus pris en charge.

## D-015 : espace d'adressage invité (30 oct. 2026)

- Décision : `mmap` d'une réservation de 4 GiB (PROT_NONE, MAP_NORESERVE). L'adresse invitée `a` est à l'adresse hôte `base + a`. L'image CT3 est chargée à l'adresse invitée `0x400000`.
- Révision : D-006 et la section 8 de `strategie.md` évoquaient un `mmap` à adresse hôte fixe `0x400000`. Ce n'est plus nécessaire : le chargement à `0x400000` est garanti dans l'espace invité, ce qui évite de réclamer une adresse basse de l'hôte.

## D-016 : code généré et export p-code non versionnés (30 oct. 2026)

- Décision : le p-code exporté (`build/g2/pcode.json`) et le C généré (`build/g2/gen/`) sont produits dans `build/` et ne sont pas versionnés. Ce sont des dérivés du binaire du jeu.
- Versionné : uniquement la liste des adresses de test (`tests/g2/functions.txt`).

## D-017 : sources de temps du jeu (30 oct. 2026)

- Constat : `FUN_00407EC0` lit le compteur de cycles (`rdtsc`). `FUN_0042D220` utilise `QueryPerformanceCounter`. `timeSetEvent` est utilisé pour la minuterie.
- Conséquence : le timing de la boucle de jeu dépend du TSC. Il faudra décider, en phase 3, s'il est remplacé par une horloge liée au temps réel (approche retenue par défaut) ou émulé à un pas fixe. Consigné comme risque R3.

## D-018 : G3, traduction complète et compilation (30 oct. 2026)

- Périmètre : les 4 448 fonctions internes de Ghidra. Pas seulement les fonctions atteignables (elles sont 285 par appels directs, mais la couverture réelle dépend des appels indirects).
- Résultat : 16 objets compilés sans erreur. 240 310 sites non pris en charge, principalement x87 (`varnode` de 10 octets), opérations flottantes et `CALLOTHER`.
- `BRANCHIND` n'est pas traité par un `dispatch` : un saut dans une table de `switch` resterait dans la fonction. Il est compté comme non pris en charge, en attendant une vraie résolution.
- Limite notée : le code généré est volumineux (153 Mo) car les drapeaux ne sont pas encore éliminés quand ils sont inutilisés.
- Détail : `docs/g3_rapport.md`. Script : `tools/recomp/run_g3.sh`.

## D-019 : jeu d'instructions processeur (proposition, à décider)

- Constat : le jeu contient des instructions 3DNow! et SSE, et il interroge `CPUID` 170 fois.
- Option 1 : implémenter x87, SSE et 3DNow! à l'identique. Fidélité maximale, plus de travail.
- Option 2 : faire répondre `CPUID` comme un processeur sans 3DNow! ni SSE, pour que le jeu prenne son chemin x87. Moins de travail, mais comportement possiblement différent (rendu, physique). À vérifier sur les chemins de code concernés avant de choisir.
- Statut : non décidé.

## D-020 : x87 implémenté (30 oct. 2026)

- Modèle : registres `ST0`..`ST7` de 10 octets (offsets p-code 4352 + 16·i), décalages de pile explicites dans le p-code. Les opérations sur valeurs de 10 octets passent par des pointeurs (`rt_fget`/`rt_fput`, `long double` gcc/clang x86_64, format x87 natif).
- Mode d'arrondi : `ROUND` lit le RC du mot de contrôle (bits 10-11), donc `__ftol` (troncature par `FLDCW`) fonctionne. Les autres opérations arithmétiques suivent le mode d'arrondi du processus (par défaut : au plus proche). Le contrôle de précision (PC) n'est pas modélisé.
- Transcendantes par `CALLOTHER` (userops 105, 108, 111, 112, 113, 114, 119) : `F2XM1`, `FCOS`, `FPATAN`, `FPTAN`, `FSCALE`, `FSIN`, `FYL2X`. Correspondance établie sur les instructions du binaire, vérifiée par les tests.
- `FFREE` (tag word), `RDTSC` (TSC de l'hôte, cf. D-017), `INT3` (arrêt) : traités.
- Limites : `FPREM` (4 sites) non traité. Précision des transcendantes : libm glibc en `long double`, différente de l'x87 matériel au niveau de l'ulp.
- Vérification : `tests/x87` (cinq routines réelles de CT3.exe), `tests/g2` toujours OK.

## D-021 : harnais et CMake partagés (30 oct. 2026)

- `tests/common/harness.h` : chargement de CT3.exe, pile, registres, ST(i), check.
- `cmake/CT3Recomp.cmake` : fonction `ct3_translated_test(NAME ADDRS SOURCE)` (export Ghidra, traduction, compilation, test). Utilisée par `tests/g2` et `tests/x87`.
- Limite : une tranche de 300 fonctions au plus pour ces tests (`gen_funcs_000.c`).

## D-022 : sauts indirects et CPUID (30 oct. 2026)

- `BRANCHIND` : cibles lues dans les références de saut calculé de Ghidra (export `targets`). Dans la fonction : chaîne de comparaisons vers les labels. Hors fonction : `dispatch`. Sans cible connue : tail-call (`dispatch`) si c'est la dernière instruction de la fonction, sinon non pris en charge (3 sites : `FUN_00473650`, `FUN_00486410`, `TranslatorGuardHandler`).
- Cibles `EXTERNAL:` (imports) ignorées dans le calcul des cibles : elles passent par `dispatch` comme tail-call.
- `CPUID` (userops 45 à 61) : profil annoncé par `runtime/src/rt_cpu.c` : AMD (`AuthenticAMD`), famille 6 modèle 8, MMX, SSE, SSE2, 3DNow! et extensions (MMXExt, 3DNow!Ext). Les extensions annoncées doivent rester cohérentes avec celles implémentées (D-023).
- Vérifié : `tests/cpu` (profil), `tests/g2`, `tests/x87` toujours OK.

## D-023 : MMX, SSE, 3DNow! (option 1 retenue, 30 oct. 2026)

- Décision de l'utilisateur : implémenter 3DNow!, MMX et SSE à l'identique (option 1 de D-019).
- Modèle : `MM0`..`MM7` (8 octets) se superposent aux `ST` (offsets 4352 + 16·i) ; `XMM0`..`XMM7` (16 octets) à 4608 + 64·i ; `MXCSR` à 4244.
- Reste à faire (1 310 sites) : userops MMX (136 à 169), SSE (173, 174, 176 et opérations 16 octets : `PAND`, `POR`, `PXOR`, `PANDN`, `PMOVMSKB`), 3DNow! (232 à 256, `FEMMS` 251), `FPREM` (4 sites), `XCHG` (userops 17, 18), et 3 sauts sans cible.
- Limite à assumer : les approximations de réciproque et de racine inverse (`RCPPS`, `RCPSS`, `RSQRTSS`, `PFRCP`, `PFRSQRT`) dépendent de tables matérielles approximatives. Une reproduction bit à bit exige ces tables. Je les implémenterai avec une précision documentée et les tests vérifieront leur écart avec le calcul exact, au lieu de prétendre à une identité bit à bit.

## D-024 : référence Unicorn installée (31 oct. 2026)

- Installation : environnement Python dédié `~/.venvs/unicorn-ref` (Unicorn 2.1.4), dans le répertoire personnel. Pas de changement système, pas de distrobox (image lourde). Suppression : `rm -rf ~/.venvs/unicorn-ref`.
- Sonde : `tools/ref/probe_unicorn.py`.
- Résultats :
  - MMX et SSE exécutés correctement (PADDSW avec saturation, ADDPS, RCPPS, PMOVMSKB). Les registres MMX ne sont pas accessibles directement : chargement via MOVQ.
  - 3DNow! exécuté seulement avec le modèle `UC_CPU_X86_ATHLON` (cohérent avec le profil CPUID de D-022).
  - Les approximations (`RCPPS`, `PFRCP`) donnent un résultat exact à la précision float, pas l'approximation matérielle (environ 2^-14 pour `PFRCP`). Unicorn n'est donc pas un miroir du matériel pour ces instructions.
- Conséquence : Unicorn est une référence fiable pour les opérations exactes (entiers, saturation, arithmétique flottante). Pour les approximations, je propose de les calculer à la précision float (comme la référence), et de documenter l'écart avec le matériel (à valider).

## D-025 : opérations 3DNow! et SSE, limites de la référence (31 oct. 2026)

- Référence : Unicorn (modèle Athlon pour 3DNow!/MMX, modèle par défaut pour SSE). Vérification bit à bit sur 200 vecteurs par opération.
- Identiques à la référence : 16 opérations 3DNow! exactes, 8 MMX, 3 SSE (RCPPS, RCPSS, RSQRTSS).
- Non vérifiables avec la référence :
  - `PFRCPIT1`, `PFRCPIT2`, `PFRSQIT1` : Unicorn renvoie la source sans calcul (200 cas sur 200). Implémentés selon Newton-Raphson : `PFRCPIT1 = 2 - d·x`, `PFRSQIT1 = 1,5 - 0,5·d·x²`, `PFRCPIT2 = t·x`. Vérifiés par convergence (`check_newton`), pas au matériel.
  - `PFRSQRT` pour x < 0 : Unicorn renvoie signe(x)/√|x|. Conservé. `RSQRTSS` (SSE) suit IEEE (NaN), différent : c'est voulu.
- Précision des approximations (`PFRCP`, `PFRSQRT`, `RCPPS`, `RCPSS`, `RSQRTSS`) : exacte à la précision float, comme la référence. Le matériel est moins précis (environ 2^-12 à 2^-14). Écart non reproduit ; à valider.
- Statut : décision de l'utilisateur de garder l'option 1 (implémentation complète). Les points non vérifiés sont listés ici pour relecture.

## D-026 : couverture complète du code (31 oct. 2026)

- Résultat : 4 448 fonctions traduites, 0 site non pris en charge, 16 objets compilés.
- Sauts indirects : cibles Ghidra dans la fonction (chaîne de comparaisons), sinon tail-call `dispatch`. Une cible non traduite arrête l'exécution avec un message explicite (`rt_unresolved`), jamais en silence.
- Marqueurs `CALLOTHER` 17 et 18 (XCHG) : sans effet propre, l'échange est fait par les opérations voisines.
- `FEMMS` : tag word à « vide ». `RDTSC` : TSC de l'hôte (D-017 toujours ouvert).
- Non vérifié : `FPREM`, marqueurs XCHG, tail-calls indirects (pas de référence d'exécution).

## D-027 : démarrage depuis le point d'entrée (31 oct. 2026)

- Processus minimal : TEB (`FS_OFFSET`, registre p-code à l'offset 272), PEB, zone TLS, tas de 64 Mo (`runtime/src/rt_proc.c`).
- Zone mémoire invitée (0x10000000-0x10400000) : tests < 0x10080000, bloc CPUID 0x10080000, TEB/PEB/TLS 0x10090000, zone de travail 0x10100000-0x10300000, pile 0x10300000-0x103FFF00.
- Imports : chaque appel est tracé (`[import] (retour …) nom(args)`). Un import non implémenté arrête l'exécution avec son nom (code 3, pas de core dump).
- Les fonctions chargées par `GetProcAddress` reçoivent des talons dynamiques. Les noms de DLL sont comparés sans tenir compte de la casse ni du suffixe `.dll`.
- `GetModuleFileNameA` renvoie le chemin réel de `CT3.exe` (fourni par le lanceur). `IsProcessorFeaturePresent` répond selon le profil CPUID (D-022).
- Shims implémentés (nargs vérifiés par la convention stdcall) : version et environnement (GetVersion, GetVersionExA, GetCommandLineA, GetEnvironmentStringsW, GetACP, GetOEMCP, GetCPInfo), tas (HeapCreate/Alloc/Free/Size/ReAlloc), conversions (MultiByteToWideChar, WideCharToMultiByte : Latin-1, hors plage -> '?'), types de caractères CT3_CTYPE1 et LCMapString (minuscules/majuscules ASCII seulement), sections critiques, TLS et FLS, temps (horloge hôte), Sleep, Interlocked* (un seul fil).

## D-028 : points d'entrée découverts (31 oct. 2026)

- Ghidra n'identifie pas toutes les fonctions. Deux sources ajoutées :
  1. Pointeurs de code dans les données (tables de fonctions, `pcode-all-ptrs`) : 1 033 candidats, 732 fonctions créées, 301 rejetées (pas du code).
  2. Cibles de branchement ou d'appel direct vers une instruction d'une autre fonction (ex. continuation SEH `0x52675D` dans `FUN_00526747`) : 218 entrées, traduites comme suite linéaire jusqu'au premier RET (`interior_entries` dans `ct3rc.py`).
- Total : 5 398 fonctions traduites.

## D-029 : cibles des sauts internes à l'instruction (31 oct. 2026)

- Les cibles `const` de `BRANCH`/`CBRANCH` dans une même instruction sont des décalages relatifs à l'index de l'opération (entier signé 32 bits), pas des indices absolus. Erreur corrigée : la séquence `CPUID` bouclait sur elle-même (`P_…_33`).
- Vérification : toutes les branches de la table CPUID convergent vers l'index 67, fin de la séquence.

## D-030 : état et blocage (31 oct. 2026)

- Démarrage : le programme passe l'initialisation du CRT jusqu'à `CreateThread`, appelé par le jeu. Le runtime est monofil.
- Décision à prendre : le support des threads (émulation coopérative ou threads hôte réels avec un état CPU par fil, verrouillage du tas et des shims). C'est un choix d'architecture, à valider avant de continuer.
- Débogage : `gdb` sur `build/tests/run/ct3_run` montre la fonction traduite (`sub_XXXXXXXX`) à l'instant d'une boucle.

## D-031 : threads hôte réels, verrou réentrant (31 oct. 2026)

- Option B : chaque thread invité est un thread hôte avec son état CPU et sa pile. Un verrou global protège le tas et les shims. Les attentes (événements, sections critiques, Sleep) le relâchent.
- Le verrou est réentrant par thread (`rt_lock_enter`/`rt_lock_leave`) : un shim peut appeler du code invité (rappels de fenêtre), qui appelle d'autres shims.
- `dispatch` rend la main à l'appelant C quand la sentinelle `RT_HOST_RET` est atteinte (retour vers l'hôte).

## D-032 : fichiers Win32 (31 oct. 2026)

- Chemins relatifs au dossier de `CT3.exe`, séparateurs `\` convertis, lettre de lecteur retirée. Chaque composant existant est retrouvé sans tenir compte de la casse (les données du jeu sont en majuscules).
- `GetFileType` renvoie `FILE_TYPE_DISK` pour les handles fichiers (sinon le CRT ferme le fichier).

## D-033 : pont Direct3D 9 (31 oct. 2026)

- Les objets COM invités font 8 octets `[vtable][identifiant]`. Les vtables contiennent des adresses de talons nommés `d3d9com::Interface::Méthode`.
- Les méthodes sont générées par `tools/d3d9/gen_com.py` à partir de `d3d9.h` de dxvk-native (vtable complète de chaque interface). Les méthodes à logique propre sont écrites à la main (`runtime/src/rt_d3d9_custom.cpp`) : QueryInterface, Release, verrouillage des tampons et surfaces, création de tampons, textures, présentation.
- Les pointeurs invités sont passés directement à l'hôte (`rt_mem_base`), sauf `D3DPRESENT_PARAMETERS` (HWND 32 bits) et les structures de verrouillage (pointeurs). Les zones verrouillées passent par une zone de transit invitée recopiée à l'unlock.
- `Direct3DCreate9` initialise SDL et `DXVK_WSI_DRIVER=SDL2` avant dxvk (sinon dxvk échoue à la création).

## D-034 : repli des textures BC (31 oct. 2026)

- Si l'hôte refuse un format DXT/BC (GPU ou pilote sans compression BC, ex. llvmpipe), la texture est créée en X8R8G8B8 et les blocs écrits par le code invité sont décodés (BC1/2/3, couleurs) au déverrouillage. Sur le BC-250 (RADV), BC est pris en charge : le repli ne sert pas.

## D-035 : environnement de test (31 oct. 2026)

- La session graphique de l'utilisateur (KWin/XWayland) ne mappe plus les nouvelles fenêtres X11 (glxgears compris) : bloqué hors du projet, non modifié.
- Tests : Xvfb `:99` avec llvmpipe (`DXVK_FILTER_DEVICE_NAME=llvmpipe`). Sur Xvfb, RADV échoue avec une erreur d'E/S X (absence de DRI3). G1 réussit sur llvmpipe.
- Diagnostic : `CT3_SHOT=N` écrit le back buffer de la N-ième présentation en PPM (`CT3_SHOT_DIR`).

## D-036 : setjmp/longjmp invités (31 oct. 2026)

- `__setjmp3` (CRT MSVC, `0x51A238`) : l'appelant fait un `setjmp` hôte dans son cadre C, puis enregistre le cadre (adresse de reprise, ESP, buffer invité). `dispatch` détecte une reprise de `_longjmp` (même adresse et même ESP) et fait un `longjmp` hôte vers ce cadre (`runtime/src/rt_jmp.c`). Cadres par thread.
- Cas rencontré : chargement de `Media\Sega.png` par la D3DX (erreur de décodage PNG → longjmp).

## D-037 : tas (31 oct. 2026)

- Tas de 192 Mio. `HeapFree` réutilise les blocs par taille exacte (listes libres). Les projections de fichiers (`MapViewOfFile`) sont copiées dans le tas et libérées par `UnmapViewOfFile`.

## D-038 : audio silencieux (31 oct. 2026)

- DirectSound (`DSOUND ordinal 11`) est fourni par des objets invités silencieux : les tampons ont de la mémoire réelle (Lock/Unlock), Play/Stop ne produisent rien. Audio réel (SDL) à faire.
- DirectInput8 : objet minimal, `CreateDevice` renvoie `DIERR_DEVICENOTREG` (entrée à faire via SDL).

## D-039 : sauts indirects et export Ghidra (31 oct. 2026)

- Saut indirect dont la valeur n'est pas dans la liste Ghidra : `dispatch` à l'exécution (au lieu de `rt_unsupported`). Cas typique : `jmp [IAT]`.
- Export : un appel vers le milieu de la fonction appelante, ou une cible dans une fonction connue, devient une entrée intérieure (au lieu de créer une fonction qui découpe la fonction englobante).
- Limite connue : 4 cibles d'appel direct sans fonction traduite (`0x4A66E0`, `0x5164E5`, `0x5179B8`, `0x51B647`) ; cause non identifiée, à traiter si atteintes.

## D-040 : fenêtre et plein écran (31 oct. 2026)

- Le jeu crée le device avec `Windowed = 0` (1280×960). La fenêtre SDL est dimensionnée au back buffer AVANT `CreateDevice`/`Reset` (dxvk crée la swapchain avec la taille de la fenêtre à ce moment). Sans bordure ni déplacement : la fenêtre garde ses décorations.
- Vérifié : écrans d'intro affichés dans la fenêtre (« Created by Hitmaker », « Presented by SEGA », logo Strangelite) sous Xvfb + llvmpipe.

## Problème ouvert : plantage après environ 2 050 présentations (31 oct. 2026)

- Plantage (adresse invitée 0x2178, base NULL) dans la fonction du jeu appelée depuis `sub_004079E0` (`FUN_00428DA0` dans l'export), environ 70 s après le lancement, pendant l'intro/menu.
- Pistes écartées : threads (même plantage sans thread concurrent, `CT3_NOTHREAD`), capacités DirectSound (`GetCaps` renseignés), valeurs de retour DirectSound.
- Piste ouverte : le flux audio `Media\SE\taxi1.bin` (banque `xbhm`, 48 Mio lus) et les objets son créés depuis `FUN_0049DCF0`. Prochaine étape : un build de traçage (`-DCT3_TRACE_INSN`, enregistrement de l'instruction courante, affiché par le gestionnaire de faute).

### Piste principale (31 oct. 2026, fin de session)

- Build de traçage (`-DCT3_TRACE_INSN=1`, dossier `build-dbg/`) : le jeu se comporte différemment selon le build. Dans le build normal, `CreateWindowExA` reçoit `hWndParent = hInstance = 0x1353E20` (un pointeur du tas), dans le build de traçage `0x0` et `0x400000` (valeur attendue). Le jeu lit donc une valeur non initialisée ou un état qui dépend de la disposition du code.
- Le plantage final (`[EBX+0xE0]` ou `[base+0x2178]`, base NULL) est la conséquence probable. Piste : comparer la référence Unicorn (mémoire à zéro) au même point, et vérifier les endroits où la traduction pourrait laisser une variable non écrite (sauts vers le milieu de fonction, entrées intérieures).
- Protection ajoutée : `HeapFree` ignore les double libérations et les pointeurs non alloués (balise par bloc), sans effet sur ce plantage.

### Mise à jour (31 oct. 2026, suite)

- **Bug corrigé** : `rt_shim_call` écrivait les arguments dans un tableau de 8 mots alors que `CreateWindowExA` en prend 12 et `CreateFontA` 14 (débordement de pile hôte, comportement indéfini dépendant de l'optimisation). Tableau porté à 16, avec vérification.
- **Bug corrigé** : `LoadMenuA` renvoyait NULL alors que CT3.exe contient un menu ; le jeu déréférençait ce handle (crash en `-O0`). Handle fictif désormais.
- **Plantage précisé** : en build de traçage `-O0`, la dernière instruction invitée est `MOV [ESI],EAX` à `0x4A01F0` (boucle de création d'objets sonores dans `FUN_004A0150`). `ESI` vaut `0x2178` au lieu de la valeur de la boucle : un registre préservé par un appel a été écrasé. La source reste à trouver (surveillance matérielle de `ESI` trop lente sous gdb).
- Captures d'écran : dossier `captures/` (hors git).

### Vérification des registres préservés (31 oct. 2026)

- Build de traçage (`CT3_TRACE_INSN=1`) : après chaque appel, EBX/ESI/EDI (et EBP hors prologues) sont comparés ; écart = arrêt avec le site et la cible (`rt_regs_check`).
- Exceptions légitimes : épilogue `0x51B3CB` (retour de fonction), prologue SEH `0x51B390` (modifie EBP).
- **Bug de traduction corrigé** : `RET` servant de saut (prologue SEH `0x519D80` : `push cible ; ret`) était traduit en `return` C, donc la cible (`0x53A4B2`) n'était jamais exécutée. Chaque appel empile désormais son adresse de retour (`rt_cs_push/pop`) ; un `RET` vers une autre adresse est un saut (`dispatch`). Profondeur sauvegardée dans les cadres `setjmp`.
- Reste : le corps `0x53A4B2` sort par `jmp 0x515C71`, un épilogue de dépilage SEH (appel à `0x51BE13`) non émulé. Le plantage à 2 050 présentations subsiste (adresse 0x217C au lieu de 0x2178).
- Piste suivante : émuler le dépilage SEH (`__global_unwind2`/`_except_handler`) pour les sorties par `jmp` vers les épilogues de `try`, ou vérifier qu'une telle sortie est bien traitée par le CRT.

### Essai de contournement audio (31 oct. 2026) : échec

- Banques `Media\SE\*.bin` rendues absentes (`CT3_NOSOUND=1`) : le jeu atteint toujours 2 050 présentations, puis plante plus loin (`FUN_00414330` appelée par `FUN_004383A0` avec `ECX = [0x7CABB0] = NULL`). Le gestionnaire audio est donc nécessaire à la phase suivante. Contournement retiré.
- Diagnostic ajouté : `CT3_NOREGCHK=1` désactive la vérification des registres du build de traçage.

### Diagnostic audio : chaîne établie (31 oct. 2026)

- La machine d'états du jeu (`FUN_0042E140`, table `0x67F94C`, état dans `[0x7BC334]`) reste en état 1 pendant 1 024 images (compteur `[0x717884]`, `FUN_0042E160`). À `0x400`, elle charge les banques (`FUN_004A0150` → parseur `FUN_004162B0`), puis passe aux états 6, 7, 8, 9.
- Le parseur accepte les banques réelles : `taxi0.bin` (11 entrées) et `taxi1.bin` (5 entrées), lues en little-endian après `TMBR`. Le compteur est bien écrit dans `[ESI+0x18]`.
- L'initialisation audio `FUN_0043EC20` (qui remplit `[0x7CABB0]`) n'est pas exécutée avant le plantage : `[0x7CABB0]` reste NULL car elle n'a jamais été appelée, pas parce que la banque échoue. Ses appelants (`FUN_0043FF80`, `FUN_00440730`, points d'entrée `ENTRY_0044004B`) sont atteints par pointeurs de fonction, pas encore identifiés.
- Le plantage survient juste après le chargement : `FUN_004A0150` appelle `FUN_00415B00` (recherche et lecture d'élément) pour chaque élément, qui appelle `FUN_00415730`. Celle-ci déréférence `[élément+0]` (pointeur vers le tableau de canaux, 0x74 octets chacun), qui est invalide pour cet élément (valeur `0x2178`). L'élément n'a donc pas été initialisé par le parseur.
- Le contournement par faux élément (`CT3_HOOK_AUDIO`, abandonné) faisait passer le jeu à 2 054 présentations, puis un appel non traduit à `0x5A8FD8`, qui est une zone de données : un faux élément n'est pas une solution.
- La surveillance `CT3_WATCH` lisait des adresses hors zone et plantait ; elle est bornée depuis.

Outils de diagnostic ajoutés (build de traçage uniquement) : `CT3_PROBE=<lo>-<hi>` (sonde d'instructions, `runtime/src/rt_diag.c`), `CT3_WATCH=<adresse>` (affiche ECX, argument, état de la machine d'états).

Prochaine étape : déterminer qui remplit `[élément+0]` (tableau de canaux) pour les éléments de la banque, dans le parseur (`FUN_004162B0`, boucle `0x4164xx`) ou dans `FUN_004150D0`/`FUN_00415290`, et pourquoi c'est nul.

### Diagnostic audio, suite (31 oct. 2026)

- Constats corrigés : `[élément+0]` n'est pas nul (`0x264F2B5C`, valide). L'hypothèse « pointeur nul » est fausse.
- Le parseur lit la banque entière en mémoire quand `[0x67BE78]` est non nul (confirmé par les `fread` de 0x2E92A00 et 0x2E6800 octets). `FUN_004154A0` prend alors la branche « mémoire » : pointeur de donnée = `[EBX+0x38] + décalage + 0x40`, et renvoie la longueur (`0x2178`).
- `FUN_004146B0` (démarrage de voix) copie `longueur` octets depuis ce pointeur vers un tampon, puis appelle des méthodes `IDirectSoundBuffer` (stubs silencieux, D-038). Le plantage survient après ces appels, dans le chemin de sortie de la chaîne d'appels, dont le cadre SEH n'est pas encore expliqué.
- Le champ `+0x10` de l'enregistrement d'élément (`0x20006238`) est un pointeur du périphérique D3D : il vient d'un reste de pile non initialisé. À vérifier : sa valeur dépend de ce que le code D3D a laissé en pile, ce que notre pont ne reproduit peut-être pas.

Outils ajoutés (build de traçage) : `CT3_MEMWATCH=lo-hi` (écritures dans une plage, avec l'instruction), `CT3_MEMWATCH_VAL=hex` (écritures d'une valeur), `CT3_FILELOG=1` (ReadFile : demandé et lu), sonde `CT3_PROBE` avec registres, pile et tampon `FILE`.

### Première image hors intro (31 oct. 2026, correction de fond)

- **Décompte corrigé** : `grep -c IDirect3DDevice9::Present` compte les lignes `[import]` et `[retour]`. Les « 2 050 présentations » étaient donc environ 1 025 images réelles. Cela concorde avec les 1 024 images de l'état 1 mesurées plus tôt.
- **Bug de traduction (D-041)** : le corps d'une fonction était émis dans l'ordre des adresses. Si un bloc plus bas (atteint par saut) précédait le point d'entrée, l'appel exécutait ce bloc. Exemple : `_strchr` (entrée 0x518830) exécutait `lea; pop ebx; ret` de 0x518820. Correctif : `goto` vers l'entrée au début du corps (`tools/recomp/ct3rc.py`). Traduction régénérée : 5 594 fonctions.
- **Interface DirectSound 3D (D-042)** : `QueryInterface` renvoyait le tampon pour tout IID. Le jeu prend `IDirectSound3DBuffer` (`{279AFA86-…}`, vérifié dans CT3.exe) et appelle `SetMinDistance` (3 arguments) à l'index 17, que notre table attribuait à `SetFrequency` (2 arguments) : 4 octets nettoyés de trop à chaque appel. Une interface 3D distincte, avec les arités de dsound.h, corrige l'équilibre de pile.
- **Résultat** : premier écran hors intro, le logo « Crazy Taxi 3 » (capture `captures/frame_1300.png`).
- **Limite actuelle** : le tas invité (192 Mio) est épuisé vers 1 400 images (`tas invité épuisé`). 17 651 allocations pour 2 897 libérations, presque toutes depuis un même point du CRT (`0x516F74`). Fuite ou libération manquante à identifier.
- **Attention** : `/tmp` est un tmpfs de 4,8 Go. Les journaux de trace l'ont saturé et ont fait échouer une compilation et Xvfb. Garder les journaux de trace petits.

### Fuite mémoire : cause et correction (1er nov. 2026)

- **Pas une fuite du jeu.** À la fin de l'épuisement, 189 Mo avaient été demandés, 109,5 Mo libérés, et 79,5 Mo restaient vivants (sur 192 Mio). Le défaut était l'allocateur : un bloc libéré n'était réutilisé que pour une taille exactement égale, et la liste de libres était plafonnée à 4 096 entrées.
- **Correction (D-043)** : allocateur à classes de puissances de deux, avec découpe des blocs libres et fusion avec le bloc physiquement suivant ; le bloc de tête revient au pointeur de croissance. Les tailles restent déduites de l'en-tête (`block_total`).
- **Résultat** : le jeu passe 2 154 présentations (au lieu de 1 399), et l'écran titre reste correct.
- **Diagnostic ajouté** : `CT3_ALLOCLOG=1` journalise les allocations et libérations avec les appelants du jeu (hors plage CRT).
- **Plantage suivant** (non résolu) : vers la 2 154e présentation, `FUN_00425F90` appelle `[ECX+0x48]` sur `[EAX]`, où `EAX` est le tampon renvoyé par `FUN_00425810` (conversion de pixels). Cette fonction ne renvoie jamais d'objet COM, donc le chemin pris n'est probablement pas celui de la version d'origine : un indicateur (`[ESP+0x38]`) diffère, probablement à cause des formats ou des capacités annoncées par notre pont D3D (voir D-034).

### Plantage texture : suite (1er nov. 2026)

- **Corrigé (D-044)** : `wsprintfA` ne gérait pas `%S`. Dans les fonctions `wsprintf` de Win32, `%S` est une chaîne **large** (UTF-16) pour la version ANSI. Notre formateur recopiait `%S` sans consommer l'argument, et le jeu ouvrait des fichiers nommés `%S`. Corrigé dans `guest_format` (`runtime/src/rt_extra.cpp`) ; les ouvertures `%S` ont disparu.
- **Vérifié, pas en cause** : le tas (`CT3_HEAPCHK=1`, vérification après chaque opération) reste cohérent ; le bloc renvoyé par `HeapAlloc(0x8000)` est bien celui que renvoie `FUN_00425810`.
- **Divergence identifiée (non résolue)** : dans `FUN_00425F90`, entre `LEA EAX,[ESP+0x18]` (0x426251, sortie ppTexture) et la lecture `MOV EAX,[ESP+0x1c]` (0x42643A), l'équilibre de pile ou les registres diffèrent du natif. `FUN_004C8B84` a un chemin d'erreur (`D3DERR_INVALIDCALL`, `RET 0x20`) qui n'est pris que si `[EBP+8]` ou `[EBP+0x24]` vaut 0. Notre exécution le prend ; le natif prend `RET 0x24`. Le premier argument vient d'un registre (`EDX`) qui n'est pas couvert par la vérification des registres préservés (caller-saved). Prochaine étape : tracer la valeur de `EDX` à l'entrée de `FUN_004C8B84`, puis remonter sa source.
- **Fichiers de diagnostic** : `CT3_FILELOG=1` journalise aussi les ouvertures de fichiers ; `CT3_HEAPCHK=1` vérifie le tas ; sonde `CT3_PROBE` avec `[esp]` et `[esp+0x40]`.

### Plantage texture résolu : wrappers COM réutilisés (1er nov. 2026, D-045)

- **Cause** : `rt_com_wrap` renvoyait le wrapper existant pour une adresse hôte sans vérifier l'interface. L'adresse hôte d'une texture libérée sans passer par `Release` a été réutilisée pour une autre interface : le jeu appelait la vtable d'une autre interface (`[vtable+0x48]` hors table, valeur 0x58).
- **Correctif** : si l'interface demandée diffère du wrapper existant, un nouveau wrapper est créé avec la bonne vtable (`runtime/src/rt_d3d9.cpp`). `CT3_COMDBG=1` signale ce cas.
- **Résultat** : le jeu dépasse 3 100 images sans erreur. Écran titre avec « PRESS START » (texte dessiné). Capture : `captures/frame_2000_press_start.png`.
- **Reste à faire** : le son est toujours muet ; la suite (menus, vidéo) demande une entrée (« PRESS START ») : DirectInput est un stub, l'entrée passe par les messages de fenêtre.

### Entrée clavier et progression au-delà de « PRESS START » (1er nov. 2026, D-046 à D-049)

- **Clavier DirectInput (D-046)** : `EnumDevices` appelle le callback du jeu avec le clavier (`GUID_SysKeyboard`) ; `CreateDevice` renvoie un périphérique clavier ; `GetDeviceState` remplit les 256 octets DIK à partir des événements SDL (`runtime/src/rt_win.cpp`, table SDL→DIK). Cause : la fonction de lecture par image (`FUN_004A1790`) ne reçoit aucun périphérique, donc « PRESS START » n'est jamais détecté.
- **Trampoline non traduit (D-047)** : `0x451000` est un `jmp 0x44DC50` isolé (thunk), non reconnu par l'export ; la vtable de la classe du jeu (`0x5940BC+0x18`) pointe vers lui. Un saut direct ne modifie pas la pile : `rt_unresolved` suit les `jmp rel32/rel8` avec un signalement. Les autres cibles non traduites restent fatales. Cause réelle à corriger plus tard dans l'export Ghidra.
- **Tas (D-048)** : le tas passe de 192 Mio à 960 Mio, à `0x40000000` (sous les stubs à `0x7F000000`). Le chargement d'un nouvel écran épuisait 192 Mio (les banques sont lues entièrement en mémoire).
- **Résultat** : après Entrée (`CT3_KEY_AT=N` injecte la touche à la N-ième image, pour les tests), le jeu passe les états 3 puis 6, 7, 2, 3 et ouvre les fichiers de niveau (`fcvdatalv.bin`, `bank11.mdl`). Plus de plantage observé jusqu'à 4 150 images.
- **Limite actuelle** : les menus écrivent leur texte avec GDI (`ExtTextOutA`, 190 appels) ; notre bridge ne rend pas ce texte. L'écran à 3 800 images est noir.
- **Audio** : le jeu ouvre des `.aif`, `.ogg` ; la sortie son reste muette (DirectSound stub).

### Texte GDI : polices et atlas (1er nov. 2026, D-050)

- **Implémentation** : polices hôte rendues par stb_truetype v1.26 (`runtime/third_party/stb_truetype.h`, domaine public) ; le fichier de police est choisi par `fc-match` (fontconfig), avec repli sur Liberation Sans (`runtime/src/rt_font.c`). DC, polices et DIB deviennent des objets suivis (`runtime/src/rt_extra.cpp`) ; `ExtTextOutA` dessine les glyphes dans le DIB sélectionné (32, 24 et 1 bit ; le 1 bit sert aux atlas de police).
- **Constat** : le jeu construit un atlas de police monochrome 256×256 au démarrage (95 dessins avec DIB). L'atlas est correct (`captures/gdi_atlas_police.png`). Les 190 appels `ExtTextOutA` incluent 95 appels sans DIB (ignorés, normal).
- **Pas encore visible** : aucun texte de menu à l'écran. Après « PRESS START », l'écran reste noir avec « PRESS START » seul ; le menu qui utiliserait l'atlas n'apparaît pas encore. Cause non identifiée (état du jeu à suivre).
- **Diagnostic** : `CT3_GDIDBG=1` (chaque texte), `CT3_ATLAS_DUMP=<fichier.pgm>` (atlas), `CT3_SHOT_EVERY=N` (capture périodique des écrans).

### Menu toujours absent : décisions et constats (1er nov. 2026, D-051 à D-054)

- **D-051 : curseur de lecture DirectSound modélisé sur l'horloge** (`runtime/src/rt_extra.cpp`). Avant : `GetCurrentPosition` renvoyait toujours 0, `GetStatus` toujours « arrêté », `Play`/`Stop`/`SetCurrentPosition` ne faisaient rien. Le thread audio du jeu (`0x413560`, qui appelle `GetCurrentPosition` toutes les 250 ms) ne voyait donc jamais progresser la lecture. Après : `Play` démarre un curseur (176 400 octets/s, horloge monotone, bouclage si `DSBPLAY_LOOPING`), `Stop` et `SetCurrentPosition` le modifient, `GetStatus` renvoie `DSBSTATUS_PLAYING`/`LOOPING`. Le blocage en état 7 n'est pas levé pour autant (voir D-054), mais le comportement est plus proche d'un vrai périphérique. Choix retenu : modèle à horloge, sans sortie son encore (SDL reste prévu, D-008).
- **D-052 : export Ghidra, fonctions après remplissage `int3`** (`tools/ghidra/ExportFunctions.java`). Un point aligné sur 16 octets, précédé d'un `int3` et sans fonction, devient candidat (convention MSVC). Cas réel : `0x4519D0`, atteint par appel indirect depuis une table, n'était pas une fonction. Les entrées créées pendant le traitement sont aussi exportées (sinon elles étaient perdues). Résultat : 5 743 fonctions traduites, compilation sans erreur. Les candidats rejetés sont comptés dans `export.log`.
- **D-053 : trace des imports désactivée par défaut** (`CT3_IMPORTS=1` pour l'activer). Elle produisait des journaux de plusieurs Go (saturation de `/tmp`). `CT3_FRAMESTAT=1` donne un résumé par image (dessins, primitives, `Clear`, textures liées), léger.
- **D-054 : constats sur le noir et le logo** (conclusions, pas une correction) :
  1. Les images noires ne viennent pas d'une texture corrompue : les textures du logo sont en DXT1 et copiées telles quelles (`o->dxt = 0`, aucun déverrouillage ne les touche).
  2. Les images noires viennent du jeu lui-même : il ne dessine pas le logo (1 seul dessin de 2 primitives au lieu de 3 ou 4). Corrélation vérifiée image par image : 3 dessins (6 primitives) ou 4 (28, avec « PRESS START ») = logo visible ; 1 dessin = écran noir ou « PRESS START » seul.
  3. En sous-état 2 de l'état 3, le jeu ne dessine plus le logo et aucun menu : hypothèse non vérifiée : des ressources de menu non chargées (le chargeur n'a pas fini), plutôt qu'un problème de rendu.
  4. Le fondu (`[0x7178A8+0x54]`, valeur 255 puis 0) explique seulement l'état 7 ; il ne rend pas le titre noir (il vaut 0 pendant tout le titre visible).
  5. **Blocage de l'état 7** : reproductible seulement avec la trace d'imports (timing). L'état 7 attend `[0x717889]`, mis à 1 par `0x42D930` quand le chargeur (`[0x71EF8C]`, piloté par `0x440DA0`/`0x440280`) avance. Le chargeur reste à 0. Piste : un thread de chargement (`0x413E10`) ou un événement qui dépend du temps réel.
  6. **Piste écartée** : empreintes de texture par `LockRect` en lecture seule (`CT3_TEXHASH`, retiré) : résultat non fiable, la lecture renvoie une mémoire de transit de dxvk.
- **État** : le menu n'apparaît pas. Prochaine étape recommandée : identifier pourquoi `[0x71EF8C]` reste à 0 (quel thread doit le faire avancer, et pourquoi il ne tourne pas à temps), puis vérifier que le logo et le menu sont redessinés.

### Blocage de l'état 7 : boucle FPREM, durée des sous-états (2 nov. 2026, D-055 à D-056)

- **D-055 : `FPREM` et `FPREM1` traités par un helper exact** (`runtime/include/rt_fp.h`, `rt_fprem`). Cause réelle du blocage : la `fmod` du runtime C de MSVC (fonction `0x518904`) est une boucle `FPREM ; FSTSW ; SAHF ; JP`, qui recommence tant que C2 vaut 1. La p-code de Ghidra ne modélise pas les bits C0 à C3 de `FPREM` : C2 n'était jamais remis à 0, et la boucle tournait sans fin dès qu'un calcul précédent avait laissé C2 à 1. Le traducteur (`tools/recomp/ct3rc.py`) remplace désormais ces deux instructions par `rt_fprem`, qui calcule le reste exact (`fmodl`, `remainderl`), met C2 à 0 et renseigne les bits de quotient. Effet observé : le blocage à la 935e image de l'état 3 sous-état 2 disparaît.
- **D-056 : durées des sous-états du titre en temps réel.** `[0x7C3C34]` est recalculé à chaque image à partir de `timeGetTime` (fonction `0x47A5C0`, facteur 0,06 unité par milliseconde). La table `0x53DD84` donne donc des durées en unités : 5 099 (≈ 85 s), 4 738 (≈ 79 s), 5 509 (≈ 92 s). Un appui sur Entrée force la fin du sous-état (le compteur reçoit durée + 2). Conséquence : les durées ne dépendent pas du nombre d'images, et un ralentissement ne bloque pas le titre.
- **Constat, ouvert** : sous-état 2, le jeu finit par bloquer dans un `LockBuffer` (tampon de sommets) qui attend une fence DXVK. Les threads llvmpipe sont inactifs : le GPU ne termine pas un travail déjà soumis. Blocage reproductible à la même étape (environ 1 900 images dans le sous-état 2), dans l'environnement de test (Xvfb, llvmpipe). Non résolu.
- **Constat, écarté** : le `sched_yield` ajouté dans `rt_unlock` pour favoriser le travailleur de chargement n'a rien changé (même durée, 10 s pour 8 tâches) ; annulé.

### Premier appui à la trame 1200 : accès au menu principal (2 nov. 2026, constat, pas une décision de code)

- Les captures attribuées précédemment à la sélection des voitures (trames 6 900 et 7 200) sont en réalité la
  cinématique d'introduction, affichée avant le premier appui sur Start (confirmé par l'utilisateur, texte
  « PRESS START » présent). Ce constat invalide l'étape « sélection des conducteurs » rapportée plus haut.
- Démarrage observé par l'utilisateur : logos, écran titre, cinématique si Start n'est pas pressé, puis retour au
  titre, en boucle. La cinématique ne s'affiche qu'à la troisième fois, et seulement une dizaine de secondes. Elle
  est dessinée avec les graphismes du jeu.
- **Premier appui à la trame 1200** (`CT3_KEYS=1200`) : le jeu passe au menu principal « MODE SELECTION », qui
  reste affiché et navigable. Hypothèse (non vérifiée) : l'état 7 démarre vers la trame 1026 (fin de l'état 1 de
  1025 trames) et appelle le contrôle des touches pendant ses 300 premières trames ; l'appui à 1200 tombe dans
  cette fenêtre.
- Non traité : cinématique tronquée, écran noir après un Start pris pendant la cinématique, et délai d'environ
  71 s avant le premier logo chez l'utilisateur (environ 20 s mesurées ici avant la première trace).

## Environnement (31 oct. 2026)

- KWin (session Plasma/Wayland) ne mappe plus les fenêtres X11 nouvelles : constaté sur G1, glxgears et CT3. Une commande de rechargement de KWin (`org.kde.KWin.reconfigure`) a été lancée par erreur, sans effet visible. Non traité : redémarrer la session graphique (déconnexion/reconnexion) devrait suffire, à confirmer par l'utilisateur.
- Xvfb (`DISPLAY=:99`) : RADV échoue (erreur d'E/S X, absence de DRI3). Test avec `DXVK_FILTER_DEVICE_NAME=llvmpipe`.

## Points ouverts

- Traducteur : source sémantique à choisir. Proposition : export p-code via Ghidra headless (Ghidra 12.1.4 déjà installé). Alternative : capstone et traducteur maison.
- D-019 : résolu (option 1). Les points non vérifiés sont listés en D-025 et D-026.
