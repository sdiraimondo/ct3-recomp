# Pack de données CT3 (lot 1)

Le code x86 de `CT3.exe` n'est jamais exécuté : `tools/recomp/ct3rc.py` le traduit
en C. Le runtime a en revanche besoin de l'**image mémoire** du jeu (en-têtes PE,
`.rdata`, `.data`, `.rsrc`) et de sa **table d'imports**. Le pack extrait ces
données une fois pour toutes, à partir de votre propre copie de `CT3.exe`.

> Le pack est un dérivé du binaire du jeu. Il ne doit être ni versionné, ni
> distribué, ni joint à un rapport de bogue. `build/` et les noms d'artefacts
> ci-dessous sont déjà couverts par `.gitignore`.

## Génération

```sh
cmake -S . -B build -G Ninja -DCT3_EXE="/chemin/vers/CT3.exe"
cmake --build build --target ct3pack
```

Sortie par défaut : `build/pack/`.

| Fichier | Rôle |
|---|---|
| `ct3_data.bin` | charge utile binaire, sections alignées sur 16 octets |
| `ct3_image.h` | constantes, table des sections et table des imports, en C |
| `ct3_image.json` | manifeste lisible : empreintes, sections, imports, statistiques |

Options CMake :

- `CT3_PACK_DIR` : dossier de sortie (défaut `build/pack`).
- `CT3_PACK_INCLUDE_TEXT=ON` : inclut `.text` (débogage uniquement ; le code
  traduit n'en a pas besoin).
- `CT3_PACK_INCLUDE_RSRC=OFF` : exclut `.rsrc`.

Appel direct de l'outil :

```sh
python3 tools/pepack/pepack.py "/chemin/vers/CT3.exe" build/pack \
    --scan-generated build/g3/gen --strict --self-test
```

`--expect-sha256 <empreinte>` refuse un autre binaire que celui attendu.
`--dry-run` analyse sans écrire.

## Format de `ct3_data.bin`

Les sections sont copiées dans l'ordre du PE, chacune alignée sur 16 octets.
`ct3_image.h` donne pour chacune :

- `va` : adresse invitée absolue (base `0x00400000` comprise) ;
- `vsize` : taille virtuelle, BSS comprise ;
- `offset` : offset dans `ct3_data.bin` (0 si la section est absente) ;
- `raw_size` : octets réellement présents dans le pack (0 si absente) ;
- `prot` : `1` = lecture, `2` = écriture, `4` = exécution ;
- `present` : `1` si les données sont dans le pack, `0` si la section est
  exclue et doit être laissée à zéro.

Politique par défaut :

| Section | Présente | Raison |
|---|---|---|
| `headers` | oui | en-têtes PE, lus par certaines routines |
| `.rdata` | oui | chaînes, tables, constantes |
| `.data` | oui | globales initialisées ; la partie au-delà de `raw_size` est du BSS |
| `.rsrc` | oui | ressources Win32 (5 Ko) |
| `.text` | non | code x86 traduit en C ; jamais exécuté |

Le manifeste contient aussi `source.sha256` (empreinte de `CT3.exe`) et
`image.payload_sha256` (empreinte de `ct3_data.bin`), ce qui permet de vérifier
qu'un pack correspond bien au binaire utilisé.

## Contrat de chargement (lot 2)

Le futur chargeur de pack doit :

1. réserver `CT3_IMAGE_SIZE` octets à l'adresse invitée `CT3_IMAGE_BASE` ;
2. zéro-remplir toute l'image (le BSS de `.data` en dépend) ;
3. pour chaque section `present = 1`, copier `raw_size` octets depuis
   `ct3_data.bin + offset` vers `base + va`, puis appliquer `prot` ;
4. laisser à zéro les octets `[va + raw_size, va + vsize)` des sections
   présentes (BSS) ;
5. résoudre les imports dans l'ordre de `ct3_pack_imports` : l'entrée IAT
   `base + iat_rva` reçoit le talon `RT_STUB_BASE + 16 * index` (l'ordre est
   significatif) ;
6. démarrer à `CT3_IMAGE_ENTRY`, qui est déjà une adresse absolue.

Le pack ne contient aucun code exécutable : `CT3_IMAGE_HAS_TEXT` vaut `0` et
`.text` est marquée `present = 0`.

## Vérification des adresses

`ct3pack-check` relit le C généré (`build/g3/gen`) et vérifie que chaque adresse
statique tombe dans une section packée :

```sh
cmake --build build --target ct3pack-check
```

Résultat attendu sur la copie de référence (4 nov. 2026) :

```text
[check] 4574 adresses uniques, 31273 références :
        packed=4572 excluded=0 gap=0 outside=2
[check] adresses hors image (ignorées par défaut) : 0x00008000, 0x0c000000
[self-test] pack cohérent
```

Les deux adresses hors image ne sont pas des données du jeu : `0x8000` est une
constante utilisée par du code mort, `0x0c000000` une adresse MMIO. `--strict`
échoue si une adresse tombe dans une section exclue ou dans un trou de l'image ;
`--strict-outside` échoue aussi pour ces deux constantes.

## Limites du lot 1

- Le runtime actuel (`runtime/src/rt_pe.c`) charge encore `CT3.exe` : le pack est
  produit et vérifié, mais pas encore consommé. C'est l'objet du lot 2.
- Les imports sont décrits par RVA d'IAT ; le nom de la DLL et le nom (ou
  l'ordinal) de la fonction sont conservés. Les imports par ordinal sont
  possibles.
- Le pack ne remplace pas les fichiers du jeu (`avi/`, `Media/`, etc.), qui
  restent lus dans le dossier d'installation.
