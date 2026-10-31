# G3 : rapport de traduction de CT3.exe

Date : 30 octobre 2026. Script : `tools/recomp/run_g3.sh`. Sorties complètes dans `build/g3/` (non versionnées).

## Critère

Traduire toutes les fonctions, produire un rapport chiffré, compiler le C généré sans erreur. L'exécution n'est pas dans le périmètre de G3.

## Résultat

| Élément | Valeur |
|---|---|
| Fonctions traduites | 4 448 (toutes les fonctions internes Ghidra, dont 46 thunks) |
| Instructions | 351 509 |
| Opérations p-code | 1 800 200 |
| Lignes de C générées | 1 858 039 (153 Mo, 15 fichiers) |
| Fichiers objets compilés | 16, 0 erreur (clang -O1) |
| Sites non pris en charge | 240 310 (13,4 % des opérations p-code) |

Compilation : environ 1 min 50 sur 2 cœurs.

## Sites non pris en charge

Par opération p-code :

| Opération | Sites | Lecture |
|---|---|---|
| Varnode de 10 octets (x87) | 187 869 | Registres `ST(i)` et mémoire 80 bits. Majoritaire. |
| `FLOAT2FLOAT` | 26 138 | Conversions flottantes (x87, SSE) |
| `FLOAT_MULT`, `FLOAT_ADD`, `FLOAT_SUB`, `FLOAT_DIV`, `FLOAT_NEG`, `FLOAT_ABS`, `FLOAT_SQRT` | environ 14 000 | Arithmétique flottante |
| `FLOAT_NAN`, `FLOAT_LESS`, `FLOAT_EQUAL`, `FLOAT_NOTEQUAL`, `FLOAT_LESSEQUAL` | environ 8 000 | Comparaisons flottantes |
| `INT2FLOAT`, `TRUNC`, `ROUND` | environ 1 900 | Conversions entières/flottantes |
| `CALLOTHER` | 1 643 | Instructions spéciales (`RDTSC`, `CPUID`, etc.) |
| Varnode de 16 octets (SSE) | 613 | Registres XMM |
| `BRANCHIND` | 296 | Sauts indirects (tables de `switch`) |

Par mnémonique x86 : `FLD` 98 731, `FSTP` 67 625, `FCOMP` 16 019, `FILD` 10 490, `FMUL` 9 122, puis les instructions **3DNow!** (`PFMUL`, `PFADD`, `PFRCP`, `FEMMS`, etc.) et **SSE** (`MULPS`, `ADDPS`, etc.).

Autres observations :

- 170 instructions `CPUID` : le jeu choisit des chemins de code selon le processeur.
- Transcendantes x87 présentes : `FSINCOS` (830), `FPATAN` (376), `FYL2X` (135), `FSIN`, `FCOS`.

## Couverture depuis le point d'entrée (`0x518A9A`)

Calculée sur le graphe d'appels directs, qui sous-estime la couverture réelle :

- fonctions atteignables par appels directs : 285 ;
- dont 7 avec au moins un site non pris en charge ;
- 238 sites d'appels ou de sauts indirects (non résolus) dans ces fonctions ;
- 2 cibles d'appels absentes de la traduction : `0x4021DB` (depuis `0x402040`) et `0x51D3D5` (depuis `0x51D360`). Probablement des entrées de fonctions que Ghidra n'a pas découpées.

Deux des fonctions atteignables contiennent des `INT3` (`_abort` et `___security_error_handler`). C'est un point d'arrêt volontaire, facile à traiter.

## Ce que ce rapport ne dit pas

- Aucune exécution : la traduction est compilable, mais son comportement n'est pas vérifié au-delà de G2.
- Le code généré n'élimine pas encore les calculs de drapeaux inutilisés. C'est la principale cause de la taille (153 Mo). Ce sera à traiter si la compilation ou la mémoire deviennent un problème.
- Les appels indirects (vtables D3D, fonctions passées en paramètre) ne sont pas résolus statiquement. Leur couverture réelle ne peut être mesurée qu'à l'exécution.

## Suite proposée

1. **x87** (le plus gros bloc). Représentation des registres `ST(i)` en 80 bits (`long double` de gcc/clang sur x86_64), opérations de base, puis transcendantes.
2. **`CALLOTHER`** : `RDTSC` (voir D-017), `CPUID`, et les autres.
3. **`BRANCHIND`** : tables de `switch`, par analyse de la table ou énumération des cibles.
4. **SSE et 3DNow!** : à évaluer. Le jeu peut choisir un chemin x87 si `CPUID` ne signale pas ces extensions. C'est un choix de fidélité à décider.
5. Puis `FUN_004079E0` comme test d'intégration du rendu.

## Mise à jour : x87 (30 oct. 2026, même jour)

Après implémentation de l'x87 (`runtime/include/rt_fp.h`, `tools/recomp/ct3rc.py`) :

| Élément | Avant | Après |
|---|---|---|
| Sites non pris en charge | 240 310 | 1 773 |
| Opérations x87 (varnodes 10 octets, `FLOAT_*`, `ROUND`, `TRUNC`, transcendantes) | 187 869 + environ 40 000 | 0 (sauf `FPREM`, 4 sites) |
| Objets compilés | 16, 0 erreur | 16, 0 erreur |

Restants (1 773 sites) :

- `BRANCHIND` : 296 sauts indirects (tables de `switch`). Non résolus.
- `CPUID` : 170 sites, réponse à décider (D-019).
- 3DNow!, MMX et SSE : `PFMUL`, `PFADD`, `PFACC`, `PMOVMSKB`, `PMADDWD`, `RCPPS`, `RSQRTSS`, etc. (`CALLOTHER` 136 à 256, valeurs de 16 octets).
- `FEMMS` (74 sites, 3DNow!), `FPREM` (4 sites), `XCHG` (2 sites, `CALLOTHER` 17 et 18), `INT3` et `RDTSC` gérés.

Tests x87 sur routines réelles de CT3.exe (`tests/x87`) : `__ftol` (troncature), `FILD`/`FST`, `FADD`/`FMUL` mémoire, `FUCOMPP` + `FNSTSW` + `FSQRT` (y compris le cas NaN), 2^x (`F2XM1`, `FRNDINT`, `FSCALE`). Résultat : `X87 OK`.

## Mise à jour : partie 3 (MMX, SSE, 3DNow!, sauts) — 31 oct. 2026

| Élément | Avant | Après |
|---|---|---|
| Sites non pris en charge | 1 773 | **0** |
| Fonctions traduites | 4 448 | 4 448 |
| Objets compilés | 16, 0 erreur | 16, 0 erreur |

Ce qui a été ajouté : MMX (8 opérations), 3DNow! (16 opérations exactes et 3 d'itération), SSE (3 opérations), opérations entières sur 16 octets (`PAND`, `POR`, `PXOR`, `PMOVMSKB`, décalages), `FEMMS`, `FPREM` (via troncature flottante), `CPUID` (profil AMD), sauts indirects (cibles Ghidra, puis tail-calls).

**Vérification** (`tests/vec`, `tools/ref/gen_vectors.py`) : chaque opération est comparée à Unicorn (modèle Athlon pour MMX/3DNow!, modèle par défaut pour SSE) sur 200 vecteurs (aléatoires et cas limites). Résultat : **identique bit à bit** pour les 16 opérations 3DNow! exactes, les 8 MMX et les 3 SSE, à la comparaison NaN près.

**Ce qui n'est PAS vérifié** (voir D-025 et D-026) :

- `PFRCPIT1`, `PFRCPIT2`, `PFRSQIT1` : Unicorn renvoie la source telle quelle. Implémentés avec les formules de Newton-Raphson, vérifiés seulement par leur convergence.
- `PFRSQRT` pour x négatif : convention de la référence (signe(x)/√|x|), non confirmée au matériel.
- Approximations (`RCPPS`, `RCPSS`, `PFRCP`, `RSQRTSS`, `PFRSQRT`) : résultat exact à la précision float, pas l'approximation matérielle.
- `FPREM` : implémenté d'après le p-code, sans référence.
- Sauts indirects sans cible Ghidra (3) : traités comme tail-calls ; si la cible n'est pas une fonction traduite, l'exécution s'arrête avec un message explicite.
- Couverture ≠ exactitude : la traduction est complète, mais seules les opérations ci-dessus et les routines de `tests/g2` et `tests/x87` sont vérifiées par l'exécution.
- Deux cibles de saut restent absentes de la traduction (`0x4021DB`, `0x51D3D5`).

## Mise à jour : démarrage du programme (31 oct. 2026)

- Entrées ajoutées à la traduction : 725 fonctions à partir de pointeurs de code, 218 points d'entrée intérieurs (`interior_entries`). Total : 5 398 fonctions.
- Sites non pris en charge : 0 (le compte peut varier avec les entrées ajoutées ; voir `build/g3/gen/rapport.md`).
- Premier démarrage réel depuis `0x518A9A` : le programme exécute le CRT jusqu'à `CreateThread` (voir D-030).
