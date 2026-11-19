# Règles du projet ct3-linux

Projet : portage natif Linux puis Switch de Crazy Taxi 3 par recompilation
statique (PE32 i386 → C → binaire hôte). Le dépôt ne contient que du code
original sous GPL-3.0-or-later ; les artefacts dérivés du jeu restent locaux.

## 1. Communication

- Répondre en français, de façon concise et factuelle.
- Citer les fichiers sous la forme `chemin:ligne`.
- Distinguer clairement ce qui est vérifié, ce qui est supposé et ce qui reste
  à faire.
- Ne jamais présenter une validation matérielle Switch comme faite sans matériel.

## 2. Règle absolue : sauvegarde avant réécriture

Avant toute modification d'un fichier existant, exécuter :

```sh
.opencode/scripts/backup.sh <fichier> [<fichier>...]
```

Le script crée `/tmp/opencode/ct3-backups/<horodatage>-<pid>/` avec les copies
et `manifest.tsv` (source, copie, sha256, octets, mode). Vérifier le manifeste
et conserver le chemin dans le rapport de tâche.

- Un fichier nouveau n'a pas besoin de sauvegarde ; un fichier existant, toujours.
- Ne jamais utiliser `rm`, `mv`, `truncate`, `dd`, `shred`, `sed -i`,
  `git reset --hard`, `git clean`, `git checkout --` ou `git restore`.
- Les commandes destructrices sont refusées par `opencode.jsonc` ; ce refus ne
  remplace pas la sauvegarde.
- Les snapshots OpenCode sont un filet secondaire, pas une sauvegarde.
- En cas de doute, sauvegarder avant d'agir.

## 3. Périmètre interdit

- Ne jamais modifier `CT3.exe` ni `CT3.original.exe` (lecture seule).
- Ne jamais copier les assets du jeu dans le dépôt ; ils sont lus hors dépôt
  (chemin passé en argument ou `CT3_DATA_DIR`).
- Ne jamais versionner : `ct3_data.bin`, `ct3_image.h`, `ct3_image.json`,
  `libct3gen.a`, le C généré (`gen_funcs*.c`, `gen_dispatch.c`, `pcode.json`),
  le corpus de vérification, les captures, les journaux, `dist/`.
- `dist/` est un bundle local (`local-only`) : il lie du code traduit du jeu.
- Ne pas réécrire l'historique Git publié.

## 4. État du projet à lire avant d'agir

Dans cet ordre :

1. `docs/POINT_SITUATION.md` — état courant, mesures, priorités.
2. `docs/ROADMAP.md` — jalons `L*`, `S*`, `V*`, portes.
3. `DECISIONS.md` — décisions D-001 à D-083 ; les plus récentes font foi.
4. `docs/HANDOFF.md` — passation, pièges connus, variables.
5. `docs/ORCHESTRATION.md` — rôles orchestrateur/codeur.

## 5. Build et validation

Configuration sans données de jeu :

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Avec une copie locale du jeu (ne jamais commiter le chemin) :

```sh
cmake -S . -B build -G Ninja -DCT3_EXE="/chemin/vers/CT3.exe"
cmake --build build --target ct3_run
```

Contrôles avant commit :

```sh
git diff --check
tools/check_no_game_data.sh
```

Lancement local (assets hors dépôt) :

```sh
cd dist/ct3-linux-0.1.1-linux-x86_64
DISPLAY=:0 ./ct3-linux "/run/media/stephane/BC250_USB/Games/Crazy Taxi 3"
```

- Le code de sortie du jeu n'est pas un statut : 13 = `VK_RETURN`,
  88 = `VK_X` (bouton A manette mappé sur `x`). Ne pas le traiter comme une erreur.
- Un test qui exige le jeu doit être conditionnel et ne jamais télécharger le jeu.
- Ne pas lancer de soak long sans nécessité ; le soak L1.3 se fait lors d'une
  session de jeu réelle.

## 6. Orchestration

- L'agent primaire `orchestrator` planifie, découpe, délègue, vérifie et intègre.
- Il peut lancer au plus 4 sous-agents `coder` en parallèle, sur des périmètres
  de fichiers **disjoints**.
- Les codeurs ne lancent pas de sous-agents, ne commitent pas, ne poussent pas,
  ne créent pas de tags et ne fusionnent pas.
- Deux codeurs ne doivent jamais éditer le même fichier ; en cas de recouvrement,
  sérialiser ou découper autrement.
- L'orchestrateur vérifie chaque rapport (`git status`, diff, build, tests) avant
  d'intégrer. Il ne délègue pas la vérification finale.
- Chaque prompt de codeur doit contenir le contexte complet : objectif, fichiers
  autorisés/interdits, commandes de validation, format de rapport. Les codeurs
  démarrent avec un contexte vierge.
- Commit après vérification, message en français, `git commit -s`.
- `git push` demande confirmation ; ne jamais forcer.

## 7. Definition of done

Une tâche est terminée quand :

- le code compile et les tests pertinents passent ;
- `git diff --check` et `tools/check_no_game_data.sh` sont propres ;
- les fichiers existants modifiés ont une sauvegarde et un manifeste ;
- la documentation utile est mise à jour ;
- aucun artefact dérivé n'est ajouté au dépôt ;
- le rapport indique ce qui est validé, ce qui reste ouvert et les risques.

## 8. Sécurité et publication

- Aucun secret, jeton, clé ou chemin privé dans le dépôt.
- Publication **source seule** sous GPL-3.0-or-later ; voir `LICENSE`, `LEGAL.md`,
  `PUBLISHING.md`, `THIRD_PARTY.md`.
- Ne pas joindre de données de jeu aux issues, PR, commits ou messages.
- La CI publique ne doit jamais avoir besoin de `CT3.exe`.
