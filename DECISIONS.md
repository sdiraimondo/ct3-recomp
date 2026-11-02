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

### Cinématique d'introduction : blocage de la boucle audio, corrigé (2 nov. 2026)

- **Constat** : la séquence titre -> cinématique -> titre n'est pas encore fonctionnelle. Le titre (sous-état 0, environ 85 s) se termine vers la présentation 6 500 ; le thread principal attend alors dans `0x412AB0` un acquittement du thread audio (`0x413560`).
- **Mécanique établie** : le thread audio est bloqué dans la boucle d'écriture `0x412850`, qui appelle le décodeur `0x4BD4A0` jusqu'à remplir la taille demandée. Le décodeur renvoie `-131` (`0xFFFFFF7D`) quand le nombre de trames à écrire vaut 0 (`EBX <= 0`, division de la taille demandée par la taille d'une trame). `0x412850` ne sort pas sur ce code et réessaie immédiatement : boucle sans fin, sans appel système.
- **Observations** : le tampon en anneau du décodeur (`0x52AB20`, `0x52AB80`) reste à lecture 616 / écriture 1 024 sans consommation ; le décodeur ne fait aucun appel au système audio pendant la boucle ; aucune lecture de fichier n'échoue (`CT3_FILELOG`, journal `[lecture-erreur]` ajouté, rien).
- **Hypothèse retenue** : la taille demandée par la boucle vient de nos curseurs DirectSound (`GetCurrentPosition`, D-051), qui ne sont pas alignés sur un bloc de 4 octets : l'espace libre peut valoir 1 à 3 octets, donc 0 trame.
- **Essai** : alignement des curseurs sur 4 octets. Résultat : le décodeur progresse, mais le jeu plante ensuite (faute mémoire à l'adresse invitée `0xBC8BD0C3`, pendant l'écriture audio, vers la présentation 2 980). Essai non retenu, retiré du code. Un essai antérieur (curseur d'écriture décalé de 4 096 octets) n'a rien changé.
- **Diagnostic ajouté** : `CT3_PROBE_AFTER=N` (trace instruction par instruction, build `-O0`, à partir de la présentation N), `[lecture-erreur]` dans `CT3_FILELOG`.
- **Cause réelle (D-057)** : l'écrivain en anneau du jeu (`0x412940`) découpe ses écritures aux limites du tampon. Une position non alignée sur une trame de 4 octets donne un morceau de 2 octets : le décodeur (`0x4BD4A0`) calcule alors 0 trame, renvoie `-131`, et la boucle `0x412850` réessaie sans fin. Le curseur de lecture de D-051 n'était pas aligné : corrigé (curseurs arrondis à 4 octets).
- **Second défaut (D-058)** : `IDirectSoundBuffer::Lock` ne gérait pas le verrouillage qui dépasse la fin du tampon : une seule zone était renvoyée, avec la longueur complète, donc écriture dans le tas (plantage `rt_heap_alloc` observé avec l'alignement). Corrigé : deux zones (jusqu'à la fin, puis le début), `DSBLOCK_ENTIREBUFFER` (0x2) respecté.
- **Résultat** (run sur `:99`, llvmpipe) : titre -> cinématique -> titre. La cinématique montre des scènes du jeu (environ 17 s, images 8 550 à 9 450), sans écran noir pendant la séquence. Restent : environ 12 s d'écran noir de chargement avant la cinématique (état 7, thread de textures), un écran noir après la cinématique (titre en sous-état 2, logo non dessiné), et une cinématique plus courte que prévu.
- **Diagnostic ajouté** : `CT3_PROBE_AFTER=N` (trace instruction par instruction, build `-O0`, à partir de la présentation N), `[lecture-erreur]` dans `CT3_FILELOG`.

- **Séquence des sous-états du titre (D-059, constat)** : sous-état 0 = titre (replay AC chargé, logo et « PRESS START »), sous-état 1 = cinématique LV (replay `RPY_*_ADV_LV`, scènes 3D rendues : beaucoup de changements de texture par image, dessins indexés non comptés par `CT3_FRAMESTAT`), sous-état 2 = retour au titre avec logo pendant environ 15 s, puis « PRESS START » seul sur fond noir pendant environ 80 s. Les fichiers de la cinématique SA (`RPY_DATA_ADV_SA`, `RPY_CAR_ADV_SA`, banques) sont lus en entier : le problème n'est pas un fichier manquant. Aucun dessin de monde n'est émis en sous-état 2 : la cinématique SA ne démarre pas. Cause non identifiée (condition de démarrage du replay dans l'état 3 ou l'état 7).
- **Écran noir avant la cinématique** : environ 12 s en état 7 (chargement des banques, thread de textures).

### Cinématiques : ordre et cinématique SA (2 nov. 2026, constats)

- **Ordre des sous-états** : le sélecteur `[0x71787C]` (fonction `0x42DBA0`, appelée par le gestionnaire d'état 6) suit un cycle fixe 0 -> 1 -> 2 -> 0 : titre, cinématique LV, cinématique SA, puis titre. Aucun tirage aléatoire trouvé dans ce sélecteur. `0x45A850` remet le sélecteur à 0 (appelée par `0x47A5C0`, réinitialisation de partie). Si l'ordre observé par l'utilisateur varie, la réinitialisation est la piste à vérifier.
- **Cinématique SA (sous-état 2)** : aucun `DrawIndexedPrimitive` n'est émis pendant la fenêtre tracée (6 images), contre environ 1 000 par image pour LV (sous-état 1). Le dessin du monde (`FUN_00421220`, appelé par `FUN_0041ABB0`, lui-même appelé par `FUN_0041E6A0` depuis `FUN_00435570`) n'est donc pas exécuté. Ce n'est pas un défaut de notre pont de rendu : c'est la logique du jeu qui ne lance pas la scène. `FUN_00435570` teste le sous-état (`[0x7BC354]`) : pour un sous-état non nul, la scène est mise en file dans `0x71D820` (compteur `0x71E96C`). Le compteur lu à chaque image reste à 0 y compris en LV : la file est vidée dans la même image, donc ce compteur ne permet pas de conclure.
- **Prochaine étape** : identifier la condition de `FUN_00435570` / `FUN_004342D0` qui active le rendu de scène pour SA (données de scène `SPL_SA`, `PEV_SA`, `PPL_C3`, `SObj_C3` chargées, mais peut-être non consommées).

### Cinématique SA : durée de scène (2 nov. 2026, constats)

- **Oui, deux cinématiques d'attente sont visibles dans notre run** : LV (sous-état 1) et SA (sous-état 2), entre le titre (sous-état 0) et le retour au titre. LV marche ; SA est noire après une courte phase.
- **Mesure de rendu de scène** (compteurs par image, `CT3_CALLS=1`) : en sous-état 1, `FUN_0041E6A0` / `FUN_00421220` sont actives pendant 1 389 images ; en sous-état 2, pendant 349 images seulement (sur 4 917). La scène SA s'arrête donc tôt : le noir vient de la fin de la scène, pas d'un rendu qui ne démarre pas (correction de D-060).
- **Compteur de progression** : `FUN_004342D0` lit un flottant à `[enregistrement+0x74]` (table `0x53F198`, pas `0x94`, indexée par `[objet+0x80]`), comparé à 0 (`0x53BBFC`). Les valeurs mesurées (environ 18 à 41, arrivant à 0 en fin de scène) varient selon l'objet : le compteur de fin de scène n'est pas encore isolé.
- **Données** : `RPY_CAR_ADV_*` commencent par un entier (8 pour AC, 4 pour SA, 0 pour LV) puis des positions et angles. Ce mot pourrait être un nombre d'enregistrements : non vérifié.
- **Non résolu** : la cause de la fin prématurée de la scène SA. Pistes : durées de scène tirées au hasard (`_rand`, 284 appelants), ou valeur initiale du compteur lue dans les données de replay.

### Cinématique SA : chronologie du réalisateur (2 nov. 2026, constat principal)

- **Réalisateur** : la mise à jour de scène de l'état 3 (`FUN_004404E0`) appelle `FUN_00498110` avec `[0x7CA0AC]`. Champs : `+0x18` index courant, `+0x1C` nombre d'entrées, `+0x34` pointeur de chronologie. Les valeurs sont fixées à l'initialisation (`FUN_00496C80`, argument 0 pour les cinématiques) : 80 entrées, chronologie à `0x693C60` (titre), `0x694840` (LV) ou `0x695420` (SA) selon le sous-état. La scène se termine quand l'index atteint 80.
- **Mesure (une image sur deux de sous-état)** : titre : index 5 pendant 6 806 images (attente de Start) ; LV : index 0 à 21 avec des durées courtes ; SA : index 0 à 4 seulement, avec **3 836 images bloquées à l'index 4**.
- **Entrée 4** (lecture en mots de 16 bits, entrée de 0x26 octets) : LV et titre ont des champs non nuls aux positions 12 et 14 (valeurs 81/96 et 4/1) ; SA a 0 à ces positions. Hypothèse (non vérifiée) : l'entrée 4 de SA référence une ressource (objet ou personnage) absente, donc l'événement n'est jamais satisfait, et le réalisateur attend. Les données sont statiques dans l'exécutable : la cause est soit dans la lecture que nous en faisons, soit dans une ressource que le jeu charge pour SA et que notre pont ne fournit pas.
- **Prochaine étape** : identifier ce que signifient les champs 12 à 16 de l'entrée (fonction qui évalue l'entrée 4 dans `FUN_00498110`), et vérifier si la ressource référencée est chargée pour SA (fichiers `SObj_C3.bin`, `PPL_C3.bin`, `PEV_SA.bin` ouverts en état 2).

### Cinématique SA : nature de l'attente de l'entrée 4 (2 nov. 2026, constat)

- **Logique d'avancement** (`FUN_00498110`) : le compteur `[réalisateur+0x20]` est comparé au temps de l'entrée suivante (`+0x28`). Si le drapeau d'attente `+0x14` n'est pas levé, l'entrée passe à la suivante quand le champ `+0x2C` vaut 1 ; sinon `0x474560` évalue l'événement.
- **Évaluation d'événement** (`0x474560`, `0x445390`, `0x4452A0`) : une table d'événements par sous-état (`[0x68FF34 + sous-état·4]`) donne une liste d'enregistrements de 12 octets. `0x4452A0` compare des coordonnées (flottants `+0` et `+8`) entre un enregistrement et une position de référence : c'est un test de volume (zone à atteindre), pas un temps.
- **Conséquence** : l'entrée 4 de SA attend qu'une position atteigne une zone. Hypothèse (non vérifiée) : la voiture ou la position de référence reste immobile chez nous pour SA, parce que la lecture du replay `RPY_CAR_ADV_SA` ne fait pas avancer la position. Le test à faire : suivre la position de référence (`0x71EBB4` ou l'objet passé en argument) dans les 3 800 images bloquées.

- **Mesure de la position de référence `0x71EBB4`** (`CT3_CALLS=1`, sous-états de l'état 3) : LV parcourt environ 1 260 positions différentes ; SA bouge brièvement (environ 300 positions) puis reste **figée** à `(975,9 ; -62,6 ; 513,7)` pendant plusieurs milliers d'images. La chronologie attend donc une position qui ne change plus : la cause est en amont (lecture du replay SA, ou caméra figée).
- **Nature de `0x71EBB4`** : variable globale partagée, écrite par plus de 80 routines, dont plusieurs avec des constantes par scène (`FUN_004808F0`, `FUN_00480D10`, `FUN_0047DBF0`, etc.). C'est très probablement la position de caméra ou de véhicule de la cinématique. Piste suivante : la routine qui met à jour cette position à partir des données de replay (`RPY_CAR_ADV_*`) pendant les sous-états 1 et 2, et ce qui la fige en SA.

### Cinématique SA : bascule titre / réalisateur (2 nov. 2026, état à la reprise)

- **Cycle observé dans chaque sous-état** (objet de contrôle `[0x7C9F18]`, champ `+0x8`, lu par `0x42E330`) : phase titre (`+0x8` = 2) d'environ 1 000 images ; passage à `+0x8` = 0 vers l'image 1 000 (directeur `0x4404E0` / `0x498110` actif) ; `+0x8` = 1 vers l'image 1 700 ; retour à 2 vers 1 870 ; fin de sous-état. **Le même cycle se produit en LV et en SA** : la phase réalisateur de SA existe bien.
- **Mécanisme de bascule** : `0x496360` ne remet `+0x8` à 0 que si `0x457650` (donc `0x4124B0`) renvoie 0, soit si le drapeau `[0x7164B4]` est non nul. Mesuré : ce drapeau reste à 0 pendant tout le run et l'objet de flux `[0x7164C4]` reste nul (16 000 relevés). **Ce n'est donc pas le mécanisme qui distingue LV et SA** ; le chemin de remise à 0 n'est pas celui qui est pris. Les bascules viennent des écritures de `0x497910` (index − 1) et de `0x497C80`/`0x497E40` (avance de 12 octets).
- **Constat sur SA** : l'entrée 4 de la chronologie reste bloquée et la position de référence `0x71EBB4` se fige en SA dans le run où la chronologie stagne. Le cas bloquant n'est pas reproduit à chaque cycle : une comparaison précise doit porter sur le même cycle en LV et en SA (même valeur de `+0x8`, même index de chronologie).
- **Reprise recommandée** : (1) relever à chaque image, pour LV et SA, `[0x7C9F18]+0x8`, l'index de chronologie `[0x7CA0AC]+0x18`, la position `0x71EBB4`, `[0x7BD080]`, `[0x7BD078]` ; (2) trouver la première image où SA diverge de LV ; (3) remonter la variable fautive.

### Cinématique SA : trajectoire et zone (2 nov. 2026, mesures de reprise)

- **Mesure image par image** (`ref1.err`) : en SA, la position de référence `0x71EBB4` **saute** à chaque changement d'entrée de la chronologie (images 1 100, 1 265, 1 428 : sauts vers `(5593;105;-1220)`, `(5538;3;-479)`, `(2055;0;366)`), puis suit un trajet lent qui s'arrête à `(976;-63;514)` (dernière mesure, image 5 509). La zone de l'entrée 4 n'est jamais atteinte. En LV, la trajectoire continue : 1 260 positions distinctes.
- **Entrées de la chronologie** : LV et SA ont la même entrée d'attente (identifiant d'événement 2 : valeurs `2` et `20` dans les champs). Donc même zone ; seule la trajectoire diffère.
- **Zone (événement 2)** : `0x6910A0 + 8·2` donne un couple de pointeurs vers une liste de points de 12 octets, à `0x5B5960`. Les points sont presque tous nuls, avec un arc de 8 points en `(y,z)` autour de `y ≈ −1 900`, `x = 0`. La géométrie de ce test (`0x4452A0` compare des composantes signées) n'est pas encore établie : test de franchissement de plan possible, à vérifier.
- **Conséquence** : la trajectoire SA n'atteint jamais la région de la zone. Deux causes à départager : (a) la trajectoire SA est bien tronquée par les données (replay `RPY_CAR_ADV_SA`, en-tête à 4 entrées) ; (b) notre lecture (positions interpolées, sauts de l'entrée 3→4) produit une trajectoire fausse.
- **Test suivant** : comparer les sauts de position aux entrées de chronologie de SA (image exacte du saut et index), et décoder la trajectoire de `RPY_CAR_ADV_SA` au format du jeu pour voir si elle atteint la zone.

### Cinématique SA : cause de la pause, vidéo absente (2 nov. 2026, D-061 et D-062)

- **D-061 : graphe DirectShow factice pour les vidéos absentes** (`runtime/src/rt_extra.cpp`, bloc « DirectShow »). Avant : `CoCreateInstance(CLSID_FilterGraph)` renvoyait `CLASS_E_CLASSNOTAVAILABLE`, donc aucune vidéo ne démarrait et le drapeau de fin de vidéo restait à 0. Après : le graphe est créé avec les interfaces que le jeu utilise (IGraphBuilder, IMediaControl, IMediaEvent, IMediaPosition, IBasicAudio, IBaseFilter, IPin), avec les index de vtable de DirectShow. `Run` pose un seul `EC_COMPLETE` ; `GetEvent` le restitue une fois, puis renvoie `E_ABORT`. Pas de décodage ni de son.
  - Raison : la copie ne contient que `avi/loading.avi` (D-004, D-005). Les vidéos appelées par le jeu sont `sa_ic.avi` (SA, index 0x1B), `go_bb.avi` (LV, 0x16), `go_an.avi` (AC, 0x15) (table de `FUN_004121D0`). Elles sont absentes, comme le sont `Media/ADX` et les autres vidéos.
  - Hypothèse : la fin immédiate est la plus petite hypothèse sur la durée (inconnue, fichiers absents). À revoir si les vidéos sont fournies.
  - Les arités des talons suivent la convention stdcall des méthodes COM (leçon de D-042).
- **D-062 : cause du blocage de SA (vérifiée par une expérience)**.
  1. Les cues du réalisateur sont dans deux tables statiques : `0x67F8C8` (mode 1, déclenchement si trame ≥ cue) et `0x67F8D4` (mode 0, égalité). Le séquenceur de l'état 3 reçoit `EAX` = retour de `FUN_0046C320`, c'est-à-dire le temps du replay `[0x7BD078]` (et non la trame). Les cues de SA sont donc aux temps de replay 121 (PLAY `sa_ic`), 122 (réglage), 181 (STOP), 182 (RESET).
  2. Le STOP (t=181) met la phase `[0x7C9F18]+8` à 2. Dans `FUN_0042E330`, la phase 2 empêche l'appel de `FUN_004404E0` (réalisateur) : le replay n'avance plus, donc le RESET à t=182 n'arrive jamais. `[0x7C9F18]` est le lecteur vidéo : 0 au repos, 1 en lecture (`FUN_00497870`), 2 à l'arrêt (`FUN_004978C0`), remis à 0 par `FUN_00496340`.
  3. La phase 2 ne se termine que par `FUN_00496360`. Chemin 1 : si `[0x7164B4]` ≠ 0 (fin de vidéo : `EC_COMPLETE` lu dans `FUN_004123C0` via `IMediaEvent::GetEvent`), la phase repasse à 0. Chemin 2 : tirage `o+0xC <= ftol(ST0)`, mais `FUN_00457660` renvoie 0 (`FUN_00413B30` = `XOR EAX,EAX`), donc ce chemin ne sort jamais. Sans graphe, `[0x7164B4]` reste à 0 : blocage permanent.
  4. Pourquoi LV n'est pas bloquée : son cue STOP est à t=356, et la chronologie termine son segment (t=297) avant.
  - **Expérience** (forçage temporaire de `[0x7164B4]` pendant la pause, retiré ensuite) : la pause est levée à t=181 ; l'entrée 4 de la chronologie est franchie et la chronologie passe aux entrées 5 à 24. Le second cycle reproduit le même déroulement (déterministe).
  - **Départage demandé** : ni données SA tronquées, ni lecture fausse de la trajectoire. La trajectoire est correcte (le franchissement de zone a lieu à t≈181, au point où le replay s'arrête). Le blocage venait de la fin de vidéo, absente de la copie.
  - **Correction d'analyse** : `FUN_0051551C` n'est pas le générateur aléatoire (`_rand`) mais `__ftol` (conversion du flottant de ST0 en entier, prologue `FLD`/`FISTP`, vérifié à l'assembleur). L'hypothèse « durée de scène aléatoire » des notes précédentes est fausse. Le drapeau `[0x7164B4]` n'était pas à écarter : c'est le mécanisme de sortie de pause.
  - **Test de zone (étape 3, partiel)** : `FUN_00474560(pos, id)` lit la liste d'événements de la scène (`PTR_PTR_0068FF34[sous-état]`, paires count/pointeur). `FUN_00445390` renvoie vrai si un élément (`FUN_004452A0`) rapporte « atteint » (2). L'entrée 4 de SA est de type 2 (attente d'événement) : elle ne passe à l'entrée 5 que si ce test est vrai. Le passage observé confirme donc le franchissement de zone. La géométrie exacte de `FUN_004452A0` n'est pas détaillée ici.
- **Décodage des replays (étape 1, partiel)** : `RPY_CAR_ADV_*.bin` (112 Kio) : premier dword = base d'index dans la table statique (0 LV, 4 SA, 8 AC, `FUN_0046C370`) ; blocs de 0x460 octets par segment (`FUN_00497910`, `FUN_004970B0`). Les positions de la caméra sont évaluées par `FUN_004ADFB0` / `FUN_004ADEB0` (cubique par segments). Les segments (`0x68F6D8`, 12 entrées de 7 dwords) pointent vers des blocs statiques de `.data` (ex. `0x68E538` : 150 clés, 6 canaux, durée 300). Le décodage complet de `RPY_CAR_ADV_SA` n'était pas nécessaire : le blocage est résolu sans toucher aux données.
- **Résultat** : run `ct3-shots/fix1` (`CT3_SHOT_EVERY=250`, captures hors dépôt). SA affiche des scènes de jeu (ville, véhicules, avec « PRESS START » superposé comme dans la démo d'attente) ; deux cycles titre / LV / SA / titre identiques, sans écran noir pendant SA. Tests `cpu`, `vec`, `g2`, `x87`, `g1` verts.

### Plantage « adresse non traduite 0x4749B4 » : tables de saut incomplètes (2 nov. 2026, D-063)

- **Constat** : sur l'écran utilisateur (RADV), le jeu s'arrête vers la 4e-5e minute : `rt: appel vers une adresse non traduite 0x004749b4 (retour invité 0x00000000)`.
- **Cause** : `FUN_004747C0` (contrôleur de caméra) fait `JMP dword ptr [ECX*4 + 0x474CEC]` après `CMP ECX,3`. La table compte 4 entrées (`0x4749B4`, `0x4749C6`, `0x474A19`, `0x474A4A`), mais Ghidra n'en donne qu'une (`0x4749C6`). Le code de `0x4749B4` n'est pas désassemblé : ni le traducteur ni le runtime ne peuvent l'appeler. Un audit des tables bornées (`CMP R,N` / `JMP [R*4+T]`) signale d'autres cas du même type (`FUN_0044AC80`, `FUN_004692A0`, `FUN_00457BA0`, `FUN_004ADxxx`).
- **Décision** : `tools/ghidra/ExportFunctions.java` (passe `pcode-all-ptrs`) lit les entrées bornées de chaque table de saut et les ajoute aux candidats : elles sont désassemblées et exportées comme fonctions (ou entrées intérieures). Export : 1 752 pointeurs de code, 919 fonctions créées, 203 entrées intérieures. Traduction : 5 747 fonctions, compilation sans erreur.
- **Vérification** : tests `cpu`, `vec`, `g2`, `x87`, `g1` verts ; `0x4749B4` et `0x474A19` sont désormais des fonctions exportées. Le lancement sur l'écran utilisateur dépasse le point du plantage précédent. Le contrôle complet (cycle entier) reste à faire quand la fenêtre est visible.
- **Environnement** : la session KWin ne mappe plus les nouvelles fenêtres X11 (un `xmessage` de test est aussi non mappé). Action à faire côté utilisateur : redémarrer la session Plasma.

### Manettes : traduction en touches DIK via SDL (2 nov. 2026, D-064)

- **Constats** : CT3.exe n'importe pas XInput (seul `DirectInput8Create` apparaît). La lecture par image (`FUN_00418860`) n'utilise qu'un périphérique, le clavier (`GetDeviceState`, 256 octets DIK) : il n'existe pas de chemin joystick dans le jeu. Les touches de menu par défaut (`FUN_004046A0`) sont haut 0xC8, bas 0xD0, gauche 0xCB, droite 0xCD, Entrée 0x1C, Espace 0x39, X 0x2D, S 0x1F, Tab 0x0F. Les touches de conduite sont lues depuis `TAXI3.CFG` (`FUN_004039D0`, non déchiffré).
- **Décision** : les événements de manette SDL (GameController) sont traduits en touches DIK, dans le même état que le clavier, et en messages de fenêtre (`runtime/src/rt_win.cpp`). Correspondance : croix directionnelle et stick gauche = flèches ; A et Start = Entrée ; B = Échap ; X = X ; Y = S ; LB = bas ; RB = haut. Le stick gauche n'est traduit qu'au-delà d'une zone morte de 16 000.
- **Vérification** : manette virtuelle SDL (`CT3_PAD_TEST=1`, séquence `CT3_PAD_SEQ=1200:start,1500:down,1800:a`) sur `:99`. Start ouvre le menu « MODE SELECTION », la croix directionnelle déplace le curseur, A ouvre le sous-menu « SMALL APPLE ». Tests `cpu`, `vec`, `g2`, `x87`, `g1` verts.
- **Non vérifié** : une vraie manette (aucune branchée sur la machine de développement), le stick analogique, et la conduite (touches d'accélération et de freinage, à déterminer depuis `TAXI3.CFG`). Un appui semble parfois déplacer le curseur de deux entrées : le même chemin clavier est en cause, à vérifier sur la vraie manette.

### Mapping manette PS2 et fichier Config.ini (2 nov. 2026, D-065)

- **Décision** : le mapping des manettes passe au mapping PS2 de Crazy Taxi (demande de l'utilisateur) : croix = Drive (touche S), rond = Échap (retour), carré = non attribué (klaxon à définir), triangle = Espace (Crazy Dash / changement de vitesse, à valider), Start = Entrée, L1 = marche arrière (touche X), R1 = accélérateur (flèche haut), R2 = frein (flèche bas), croix directionnelle et joystick gauche gauche/droite = diriger, joystick haut/bas non attribués (la croix directionnelle sert aux menus).
- **Constat à l'appui** : l'utilisateur avait déjà la conduite avec le mapping précédent (haut = accélérer, bas = freiner, X = marche arrière, S = marche avant) : ce sont donc les touches du jeu pour ces actions. L'écran de sélection affiche « Reverse button to accept » : dans les menus, la validation passe aussi par la touche de marche arrière (L1), en plus de Start.
- **Fichier de configuration** : `Config.ini` à la racine du dépôt (section `[Controller]`), lu au démarrage depuis le répertoire courant, ou depuis le chemin de `CT3_CONFIG`. Les valeurs absentes gardent les défauts, qui sont les mêmes que le fichier livré. Une entrée ou une touche inconnue est signalée sur la sortie d'erreur. Le fichier est prévu pour d'autres sections (résolution, réglages du jeu).
- **Vérification** : menu ouvert par Start sur la manette virtuelle, sans avertissement de configuration (`CT3_PAD_TEST=1 CT3_PAD_SEQ=1200:start`). Les touches de conduite n'ont pas été vérifiées en jeu sur cette version : seul le constat de l'utilisateur (sur l'ancien mapping) le confirme.

## Environnement (31 oct. 2026)

- KWin (session Plasma/Wayland) ne mappe plus les fenêtres X11 nouvelles : constaté sur G1, glxgears et CT3. Une commande de rechargement de KWin (`org.kde.KWin.reconfigure`) a été lancée par erreur, sans effet visible. Non traité : redémarrer la session graphique (déconnexion/reconnexion) devrait suffire, à confirmer par l'utilisateur.
- Xvfb (`DISPLAY=:99`) : RADV échoue (erreur d'E/S X, absence de DRI3). Test avec `DXVK_FILTER_DEVICE_NAME=llvmpipe`.

## Points ouverts

- Traducteur : source sémantique à choisir. Proposition : export p-code via Ghidra headless (Ghidra 12.1.4 déjà installé). Alternative : capstone et traducteur maison.
- D-019 : résolu (option 1). Les points non vérifiés sont listés en D-025 et D-026.
