# Portage natif du filtre d'image : point de reprise

Document de reprise (3 nov. 2026). Le détail chronologique et les décisions sont dans `DECISIONS.md`,
sections « Démarrage », « Portage natif », « suite », « suite 2 », « suite 3 ». Ce fichier est le résumé à lire
en premier.

## 1. Objectif et contexte

- **Problème utilisateur** : écran noir au démarrage. Le premier affichage (`Present`) arrivait environ 45 s après
  `[start]`, pendant que le jeu présente des images noires (environ 2 000 images).
- **Cause** : la routine de filtre d'image D3DX embarquée (`FUN_004DACC6`, Apply du filtre) recalcule les mip-maps
  des textures au chargement. Traduite en code hôte, elle est très lente. Profil de référence : 38 échantillons
  sur 40 dans le thread de chargement.
- **Objectif** : réduire ce temps sans changer les pixels (sortie bit à bit identique à la traduction).
- **Décisions de l'utilisateur** : piste 1 (portage natif, classe par classe) et piste 3 (accélération générale du
  traducteur). Piste 2 (cache disque du résultat) gardée en réserve, à réévaluer après les autres.

## 2. État actuel (mesures sur Xvfb `:99`, llvmpipe, build normal)

| Version | Premier `Present` après `[start]` |
|---|---|
| Traduction seule (référence) | 45,3 s |
| Version 1 : filtre `FUN_004DACC6` natif, mode de serrage 1 | 16,8 à 17,5 s |
| Version 1 + décodeur DXT1 natif | 20,5 s (plus lent : désactivé) |
| Version 1 + tramage natif (`FUN_004DDB6F`) | 10,3 s (`build-check/`) ; `build/` par défaut : 11,1 s |

- **Version 1 et tramage natif : activés par défaut**, dans `build/` (binaire utilisateur). Désactivation : `CT3_FILTER_NATIVE=0` (tout le filtre) ; `CT3_DITHER_NATIVE=0` (tramage seul).
- L'utilisateur a confirmé une amélioration nette sur son écran (version 1).
- **Tests obligatoires** (`build-check/`) : `test_cpu`, `test_vec`, `test_x87`, `test_g2`, `g1_d3d9_triangle` :
  verts après la dernière modification.

## 3. Ce qui est porté (code)

Fichier : `runtime/src/rt_filter.c` (nouveau, non commité). Points d'entrée liés à la génération :

- `rt_native_filter(cpu_t *c)` : remplace `FUN_004DACC6` (mode de serrage 1 seulement ; les autres modes restent
  sur la traduction, car non vérifiés). Déclenché depuis `dispatch` (généré) quand `rt_native_filter_on` est vrai.
- `rt_native_dxt1(cpu_t *c)` : décodeur DXT1 (`FUN_004F096A`). **Code présent, non activé** (retiré de
  `NATIVE_OVERRIDES` dans `tools/recomp/ct3rc.py`, ligne commentée). Validé sur un appel, mais plus lent que la
  traduction dans la mesure (voir section 7).
- `rt_native_dither(cpu_t *c)` : remplace `FUN_004DDB6F` (écriture X8R8G8B8 par tramage à diffusion d'erreur,
  slot 2 du record `0x5D64B0`). Porté avec `dither_prop` (`FUN_004DD45C`). Activé par défaut ; `CT3_DITHER_NATIVE=0`.
  Cas porté : `+0x54 = 0` et `+0x10 = 0` ; sinon appel de la traduction `sub_004ddb6f`.
- Helpers : `gcall` (appel d'une fonction invitée depuis le natif, ESP restauré), `gfree`, `array_ctor`,
  `filter_body` (corps du filtre), `check_call` (vérification), `fist_trunc` (`FISTP` sous `RC=11`).

Modifications liées (non commitées) :
- `tools/recomp/ct3rc.py` : table `NATIVE_OVERRIDES` (0x4DACC6 → `rt_native_filter`), émission d'un test
  `if (rt_native_filter_on)` dans `dispatch`.
- `runtime/include/rt.h` : déclarations `rt_native_filter_on`, `rt_native_filter`, `rt_native_dxt1`.
- `runtime/include/rt_proc.h`, `runtime/src/rt_proc.c` : `rt_heap_get_state` / `rt_heap_set_state` (état du tas
  côté hôte, pour la vérification).
- `runtime/CMakeLists.txt` : ajout de `rt_filter.c` à `ct3_rt`.
- `runtime/src/rt_thread.c` : une ligne, `rt_probe_dump()` dans `crash_handler` (diagnostic, sans effet par défaut).
- `tools/ghidra/DecompileFunctions.java` (nouveau) : décompilation Ghidra headless (lecture seule).
- `DECISIONS.md`, `docs/PORTAGE_FILTRE.md` (ce fichier).

### Conventions d'exactitude (à respecter pour tout nouveau portage)

- Le traducteur calcule en x87 (extended, 64 bits de mantisse). Dans une même expression, un produit ou une somme
  reste en extended jusqu'à un `FSTP` vers `float`. En C : `long double` pour les sous-expressions, `(float)` au
  point de stockage, uniquement là où l'asm stocke en mémoire.
- Une variable `float` locale de la décompilation Ghidra est en général stockée en mémoire : l'arrondi `float` a lieu
  à l'affectation. Vérifier dans l'asm (`FSTP`, `FLD` sur la même case) si doute.
- Les constantes (`0x53CC20`, `0x53BB60`, `0x53CC68`, `0x5D6464`, `0x5D646C`, `0x5DD5EC`, `0x5A70FC`, ...) sont lues
  en mémoire invitée à l'exécution, comme la traduction.
- Les appels vers le code invité (allocations `0x514666`, `0x401000`, `0x4DA5F5`, `0x5154BF`, tables de poids
  `0x4D7A41`, vtables) restent invités, dans le même ordre : le tas évolue comme avec la traduction. C'est important
  pour la reproductibilité de la disposition du tas (crash D-069).
- Le mot de contrôle x87 (`reg 4256`, 16 bits) est modifié par `FUN_004DDB6F` (`|= 0xC00` pour ses `FISTP`) puis
  restauré en fin de routine : le portage n'a pas besoin de le modifier (les arrondis sont explicites en C).
- Les erreurs de diffusion restent en extended jusqu'au `FSTP` : seul le produit `7/16` est arrondi en `float`
  (`carry`, voir `rt_native_dither`). La décompilation Ghidra montre des `float` locaux qui ne le sont pas dans l'asm.

## 4. Vérification : `CT3_FILTER_CHECK=N`

- `CT3_FILTER_CHECK=N` (avec le natif actif) : pour les N premiers appels de `rt_native_filter`, exécute d'abord la
  traduction (référence, **natif désactivé pendant cette exécution**), puis la version native sur le même état, puis
  compare : contenu du tas (`[0x40000000, sommet)`), sommet et listes libres du tas, registres EAX, EBX, ESP, EBP, ESI, EDI.
  Une différence arrête le programme (code 7) avec un message.
- Résultats : 66 appels identiques (mode 1, tailles 32×64 à 512×512), puis 4 appels identiques avec DXT1 natif.
- La comparaison couvre tous les effets de bord de la routine (écritures dans les objets, allocations), donc aussi
  les routines appelées par vtable qui écrivent dans le tas.
- Les messages de diagnostic (fourcc, classe de vtable, flags `+0x10`, `+0x54`, `+0x5C`) sont imprimés à chaque appel.
- `CT3_FILTER_CHECK_DITHER=1` : ne compte que les appels dont l'écrivain de la destination est `FUN_004DDB6F`
  (X8R8G8B8). Sans cette option, les 40 premiers appels du démarrage sont des DXT3 et ne touchent pas le tramage.
- Le harnais ignore les octets 2 et 3 de `[+0x7C]` des objets écrits par le tramage natif (indéfinis dans la traduction).
- **Limite** : n'exécute que les appels de `FUN_004DACC6` pendant le démarrage (4 à 66 appels). Les autres
  textures et modes ne sont pas couverts.

## 5. Ce que le jeu utilise au démarrage (mesuré)

- Mode de serrage : **1** (seul mode porté, vérifié).
- Format des textures : **DXT1** (`0x31545844`) et **DXT3** (`0x33545844`), FourCC en `[objet+4]`.
- Constructeur de la classe DXT : `FUN_004E5A11` (vtable `0x5D6454`, aussi écrite par `FUN_004DCA85` et
  `FUN_004E6305`). Choix selon le FourCC :
  - `DXT1` : `[+0x8C] = 0x4F096A` (décodeur de bloc), `[+0x88] = 8` (taille de bloc) ;
  - `DXT2` : `0x4F1376`, `DXT3` : `0x4F0AAA`, `DXT4` : `0x4F139A`, `DXT5` : `0x4F0B2D`, `[+0x88] = 0x10`.
  - `[+0x90]` : une autre fonction par format (non étudiée).
- Vtable de la classe : slot 0 = `0x4E5BF7`, slot 1 = `0x4E6035` (lecteur de tuiles 4×4 : `FUN_004E6035`), slot 2 =
  `0x4E5C16` (écriture ou lecture de ligne, **non encore décompilée à fond**, voir `ct3-shots/portage/decomp4.c`).
- **Objet source du filtre** (`P[0]`, `S`) : `S+0x10 = 0`, `S+0x54 = 0`, `S+0x5C` non nul (diffusion d'erreur active).
  **Destination** (`P[1]`, `D`) : `D+0x10 = 0`, `D+0x54 = 0`, `D+0x5C = 0`, `D+0x88` cohérent avec DXT.
- **Autre classe** : une partie des appels (profil) passe par une vtable dont le slot 1 est `FUN_004DDB6F` directement
  (écriture par tramage, 8 bits, appelée depuis le filtre via `gcall`). **C'est la routine qui domine le temps
  restant** (9 échantillons sur 11 dans la version avec DXT1 natif). Sa classe (FourCC, format) n'est pas encore
  identifiée : à faire en premier.

## 6. Routines et adresses utiles

| Adresse | Rôle (état de connaissance) |
|---|---|
| `FUN_004DACC6` | Apply du filtre (remplacé, mode 1). Décompilé : `ct3-shots/portage/decomp.c` |
| `FUN_004DB284` | Choisit l'implémentation de filtre (liste de candidats, le premier qui réussit gagne) |
| `FUN_004D7A41` | Construit les tables de poids (stdcall, 3 arguments) : **non porté** (appel invité, peu coûteux) |
| `FUN_004E6035` | Lecteur de tuiles 4×4 (classe DXT). Décompilé : `ct3-shots/portage/decomp3.c` |
| `FUN_004E5A11` | Constructeur de la classe DXT (choix par FourCC) |
| `FUN_004F096A` | Décodeur DXT1 (stdcall, `RET 8`), porté, désactivé |
| `FUN_004EF58F` | RGB565 vers float (fastcall, sortie dans EAX) : porté avec le décodeur |
| `FUN_004F0AAA` | Décodeur DXT3 : décompilé (`decomp4.c`), **non porté** |
| `FUN_004E5C16` | Slot 2 de la classe DXT : **à décompiler** (`decomp4.c`, lignes 125 et suivantes) |
| `FUN_004DDB6F` | Écriture X8R8G8B8 par tramage (diffusion d'erreur). **Porté** (`rt_native_dither`), actif par défaut |
| `FUN_004DD45C` | Propagation de l'erreur (ajoute `err[t+1]` au tampon, remet l'erreur à zéro). **Porté** (`dither_prop`) |
| `FUN_004E633F` | Fabrique des lecteurs/écrivains de surface : choisit le record (`0x5D6490 + 16·k`) selon le format D3D |
| `FUN_004E16E6` | Constructeur du record `0x5D64B0` (format 0x16, X8R8G8B8) : lecteur `FUN_004E3FEE`, écrivain `FUN_004DDB6F` |
| `FUN_004F0D8B` | Encodeur DXT3, bloc d'alpha. **Porté** (`rt_native_dxt3`, suite 5) |
| `FUN_004F0019`, `FUN_004EFA9B` | Encodeur DXT3, bloc de couleur et ajustement des extrémités. **Cible suivante**, traduits |
| `FUN_004DBE9F`, `FUN_004DD0FC`, `FUN_004DD326`, `FUN_004DD08C` | Drapeaux `+0x54`, `+0x10`, `+0x18` : nuls pour le jeu au démarrage, à garder en repli |
| `FUN_004E3FEE` | Lecture de ligne 8 bits ARGB vers float (rôle exact à confirmer) |
| `FUN_004D7A15` | Petit appel vers `FUN_00518930` (arrondi, utilisé par le calcul de poids) |

Constantes de tramage (lues en mémoire) : `0x53CB68` (1/255 ou échelle), `0x5D6484`, `0x5D6480`, `0x541854`, `0x5D6478`
(poids de diffusion), `0x53CC08`, `0x53CC68` (0,5).

## 7. Points ouverts et pièges connus

1. **Le décodeur DXT1 natif est plus lent** que la traduction (20,5 s contre 16,9 s). Cause non établie. Hypothèses :
   `long double` par texel, `memcpy` et conversions, appels `rdf` à chaque bloc. À profiler (échantillonnage `gdb`,
   voir ci-dessous) avant de le réactiver.
2. **`FUN_004DDB6F`** : **porté** (`rt_native_dither`, avec `FUN_004DD45C`). Points à garder : le mot de contrôle est
   mis à `|= 0xC00` pour les `FISTP` (troncature), puis restauré en fin de routine depuis `[+0x7C]` ; la valeur de
   `[+0x7C]` n'est relue que pour ce `FLDCW`. Les bits hauts de `[+0x7C]` sont indéfinis : exception dans la comparaison.
   Cas non portés : `+0x54` et `+0x10` non nuls (appel de la traduction).
3. **Multiples classes d'objets** : chaque classe a sa propre vtable et ses propres routines. Porter par classe,
   avec un garde sur la vtable (`rd32(rd32(obj)+4) == <adresse>`), comme pour le mode de serrage.
4. **Modes 2 et 3** de serrage : non portés, non vérifiés. Restent sur la traduction.
5. **Crashs en course (D-069)** : plantage A (faute à `0x3F800000`, puis `0x3F800001` sur le build normal) et
   plantage B (pointeur nul). Non résolus. Ne pas modifier la disposition du tas (voir section 3).
6. **Sortie du jeu** : plantage à la sortie (`0x5154B5`, table `atexit`) : connu, sans effet sur le jeu.
7. **Piste 2 (cache du résultat)** : à réévaluer après les portages. Clé : contenu des sources (octets lus via le
   lecteur de tuiles) + paramètres. À stocker hors dépôt (`~/.cache`), jamais dans le dépôt.

## 8. Mode d'emploi

### Compilation et tests

- Build de vérification : `build-check/` (ignoré par git). Build utilisateur : `build/` (contient la version 1).
- Les exports Ghidra (`pcode_all.json`, `g2/pcode.json`, `x87/pcode.json`) sont dans `build/` ; les build de
  vérification les référencent par lien symbolique (`REUSE_EXPORT=1`). Java n'est pas dans le PATH : passer
  `JAVA_HOME=/var/home/linuxbrew/.linuxbrew/Cellar/openjdk@21/21.0.13.0.3/libexec` pour Ghidra.
- Retraduction complète (changement de `rt.h` ou `ct3rc.py`) : environ 15 minutes.
- Tests obligatoires : `tests/cpu/test_cpu`, `tests/vec/test_vec` (sans argument), `tests/x87/test_x87` et
  `tests/g2/test_g2` (argument : chemin de `CT3.exe`, lu dans `build/CMakeCache.txt`, clé `CT3_EXE`),
  `tests/g1_d3d9_triangle/g1_d3d9_triangle` (Xvfb `:99`, `DXVK_FILTER_DEVICE_NAME=llvmpipe`).

### Mesure du démarrage

```sh
DISPLAY=:99 DXVK_FILTER_DEVICE_NAME=llvmpipe CT3_PRESENT_LOG=1 build-check/tests/run/ct3_run "$CT3EXE" 2> journal.err
# lire [start] ms=... et le premier [present] ms=... ; arrêter par PID : pgrep -x ct3_run puis kill <PID>
```

### Vérification différentielle

```sh
DISPLAY=:99 DXVK_FILTER_DEVICE_NAME=llvmpipe CT3_FILTER_CHECK=4 build-check/tests/run/ct3_run "$CT3EXE" 2> check.err
grep -a "identique\|CT3_FILTER_CHECK" check.err
```

### Échantillonnage de pile (sans outil de profilage système)

Script `ct3-shots/demarrage/sample.sh PID N SORTIE` (hors dépôt) : prélève N piles par `gdb -p`. Les fonctions
traduites portent le nom `sub_XXXXXXXX`. Traiter les piles par thread (chaque pile commence par `#0`).

### Désassemblage et décompilation

- Désassemblage depuis `build/g3/pcode_all.json` : script `asm.py` (dans `/tmp/opencode/ct3/`, peut disparaître ;
  le recréer si besoin : lire les lignes JSON `{"name": ...}`, chercher les `insns`).
- Décompilation : `tools/ghidra/DecompileFunctions.java`, avec un fichier de adresses (une par ligne, sans `0x`) :

```sh
analyzeHeadless <projet> ct3 -import "$CT3_EXE" -scriptPath tools/ghidra \
  -postScript DecompileFunctions.java <adresses.txt> <sortie.c> -deleteProject
```

Les sorties sont dans `ct3-shots/portage/` (hors dépôt) : `decomp.c` (filtre), `decomp2.c`, `decomp3.c`
(`FUN_004E6035`), `decomp4.c` (DXT1, DXT3, classe `0x4E5C16`), `decomp5.c` (`FUN_004EF58F`).

## 9. Règles du projet à respecter

- Ne jamais modifier `CT3.exe` ni `CT3.original.exe`, ne jamais copier d'assets du jeu dans le dépôt, ne jamais
  écrire le chemin de `CT3.exe` dans un fichier versionné.
- Captures et journaux hors dépôt, dans `/var/home/stephane/Dev/ct3-shots/`.
- Décisions consignées dans `DECISIONS.md`.
- Ne pas utiliser `pkill -f` : `pgrep -x ct3_run` puis `kill <PID>`.
- Ne pas lancer le jeu sur l'écran de l'utilisateur (`DISPLAY=:0`) sans son accord explicite. Les tests vont sur
  Xvfb `:99` avec `DXVK_FILTER_DEVICE_NAME=llvmpipe`.
- Les tests obligatoires doivent rester verts.
- Répondre en français.
- L'utilisateur demande de travailler en autonomie : ne revenir que pour une étape clairement terminée, ou un
  blocage réel.

## 10. État du dépôt (à la rédaction)

- Branche `master`, commit de référence `8276985`. **Aucun commit** pendant cette session : modifications non
  commitées (voir section 3). À décider avec l'utilisateur avant commit.
- Aucun processus `ct3_run` en cours.
- Le binaire `build/` contient la version 1 et le tramage natif (actifs par défaut), décodeur DXT1 absent.
- Tests obligatoires verts sur `build-check/` après la dernière modification de `rt_filter.c`.

## 11. Prochaine étape

1. **Fait (suite 5)** : bloc d'alpha `FUN_004F0D8B`, porté (`rt_native_dxt3`), 40 appels identiques, gain d'environ
   0,6 s (à confirmer). **Reste** : le bloc de couleur `FUN_004F0019` (encore traduit, appelé depuis la version native)
   et `FUN_004EFA9B`. État détaillé ci-dessous (décompilation faite, `ct3-shots/portage/decomp6.c`, asm via
   `build/g3/pcode_all.json`) :
   - `FUN_004E5C16` : slot 2 de la classe DXT (écriture d'un bloc). Gère un cache de tuiles (`[+0xF8]`, 8 octets par
     bloc : état et pointeur de tuile de 256 octets). Appelle `FUN_004F0D8B` pour le bloc d'alpha et `FUN_004F0019`
     pour le bloc de couleur (chaîne vue dans les piles : `sub_004e5c16` → `sub_004f0d8b` → `sub_004f0019` →
     `sub_004efa9b`).
   - `FUN_004F0D8B` (alpha DXT3, 16 texels sur 4 bits, diffusion d'erreur si `param_3 != 0`) : petit, à porter en premier.
     Vérifier dans l'asm les `FSTP` (arrondis) et `ROUND` (troncature sous `RC=11`, comme `FUN_004DDB6F`).
   - `FUN_004F0019` (bloc de couleur, `RC=11`, tableau local de 63 floats, appels à `FUN_004EF689` (empaquetage RGB565)
     et `FUN_004EF58F`) puis `FUN_004EFA9B` (ajustement des extrémités : divisions `1/x` et boucle de convergence à
     huit itérations maximum). Les deux sont longs : une routine à la fois, avec `CT3_FILTER_CHECK` dès que la
     dernière est portée.
   - **Suite 6 (fait)** : `FUN_004EFA9B` (ajustement des extrémités) porté (`rt_native_efa9b`), vérifié
     bit à bit (`CT3_FILTER_CHECK=8`, 100 000 ajustements), gain d'environ 2 s (à confirmer). Le bloc de couleur
     `FUN_004F0019` reste traduit : son chemin `param_2 == 0` est décrit ci-dessous, c'est la prochaine cible.
   - **Constats de lecture (suite 6)** :
     - Appel depuis `FUN_004F0D8B` : `FUN_004F0019(EBX+8, 0.0, dither)` avec `EAX` = source. Le chemin utilisé est
       `param_2 == 0.0` : `local_28 = 4` (4 couleurs, sans transparence). Les autres chemins (`param_2 != 0`) restent
       sur la traduction.
     - `FUN_004EF689` (fastcall, `ECX` = pointeur float RGB, renvoie `EAX`) : clamp dans [0, 1] (0 si négatif, 1 si
       supérieur à 1), puis `R = trunc((float)(r×31 + 0,5))`, `G = trunc((float)(g×63 + 0,5))`,
       `B = trunc((float)(b×31 + 0,5))`, sortie `(R<<11)|(G<<5)|B`. Le mot de contrôle est mis à `RC=11` pendant
       le calcul et restauré en sortie (`FLDCW` depuis la sauvegarde).
     - Constantes : `0x5D6470` = 31,0 ; `0x5D6468` = 63,0 ; `0x53CC68` = 0,5 ; `0x53BB60` = 1,0 ; `0x53BBFC` = 0,0 ;
       `0x5D646C` = 1/31 ; `0x5D6464` = 1/63 ; `0x5A70A4` = 1/15 ; `0x5DD60C` = 7/16 (float).
     - `FUN_004F0019`, chemin `param_2 == 0` : zone de travail de 0x2DC octets ; le tableau d'erreur de 64 floats
       (`[EBP-0x1E4]`) est mis à zéro seulement si `param_3 != 0` ; le mot de contrôle est écrit dans `[EBP+0xC]` (le
       slot du second argument, écrasé) puis restauré en sortie. Les résultats sont écrits via `param_1` (pointeur
       `ushort*` : deux extrémités en RGB565 et 16 indices sur 2 bits, dans le mot `[+0x4]`) ; le détail reste à lire.
     - Structure du chemin `param_2 == 0` (asm lu en entier, `ct3-shots/portage/`, `/tmp/opencode/ct3/a0019.txt`) :
       1. Boucle des 16 texels (`ECX` = offset 16·t) : `r, g, b` plus l'erreur de diffusion si `param_3 != 0`
          (zone d'erreur de 64 floats, mise à zéro avant la boucle) ; quantification `i = trunc((float)(x×31+0,5))`
          (`G` : `×63`) ; reconstruction `i×(1/31)` (`1/63` pour G) stockée dans une zone de travail ; erreur
          `x − reconstruction` diffusée avec les poids 7/16, 3/16, 5/16, 1/16 (mêmes règles de bord que
          `FUN_004F0D8B`, mais sur trois canaux).
       2. Appel `FUN_004EFA9B(&min, &max, zone, 4)` (ajustement des extrémités, voir ci-dessous).
       3. Extrémités : `FUN_004EF689` sur chaque extrémité (RGB565) ; si égales, sortie directe (`SI`, `DI`) ; sinon
          `FUN_004EF58F` sur chacune (retour en float), puis palette de 4 couleurs : `c0`, `c1`, et deux
          interpolations à 1/3 et 2/3 (constantes `0x5A70FC` = 1/3 et `0x5DD5EC` = 2/3, valeurs lues ; tables
          d'indices `0x5DD5F0` pour 4 couleurs, `0x5DD600` pour 3 couleurs).
       4. Sélection de l'indice de chaque texel (projection sur l'axe `c0`→`c1`, seuils), avec à nouveau la diffusion
          d'erreur à partir de la palette (`×0,5` avant le seuil, `0x53CC68`) ; deux bits par texel, écrits dans le
          mot `[+0x4]` de la sortie (`SHL 30`, `SHR 2`).
       5. Restauration du mot de contrôle en sortie ; `EAX = 0`.
       - **Pour le portage** : les chemins `param_2 != 0` restent traduits. Les étapes 1 et 4 sont des copies du
         motif de diffusion déjà porté (`rt_native_dither`, `rt_native_dxt3`), à transcrire canal par canal.
         L'étape 2 (`FUN_004EFA9B`, convergence jusqu'à 8 itérations) est la plus délicate : à lire avant de coder.
   - Vérification : les appels concernés ont `vt+8 = 0x4E5C16` côté destination DXT3 (`CT3_FILTER_CHECK` sans option
     vérifie les 40 premiers appels, tous DXT3 : c'est le bon jeu).
2. Ensuite, la boucle native `filter_body` (environ 35 % du temps) : vérifier la part des `long double` et des
   `gcall` avant de l'optimiser.
3. Piste 3 (traducteur) : non mesurée dans cette session.
4. Piste 2 (cache disque) : toujours en réserve.
