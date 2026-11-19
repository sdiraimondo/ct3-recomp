# Orchestration OpenCode — ct3-linux

Cette page décrit l'architecture **orchestrateur / codeur** mise en place pour
le projet : un agent primaire planifie et vérifie, jusqu'à quatre sous-agents
exécutent des tâches bornées en parallèle.

## 1. Objectif

- Paralléliser les tâches indépendantes sans mélanger les responsabilités.
- Conserver l'historique : aucune réécriture de fichier existant sans sauvegarde.
- Garder une trace vérifiable de chaque intervention (manifeste de sauvegarde,
  diff, build, tests).
- Réserver à l'orchestrateur les décisions d'intégration, les commits et les
  pushes.

## 2. Vue d'ensemble

```text
Utilisateur
   │  /orchestrate <objectif>
   ▼
orchestrator (primaire, DeepSeek V4.1 Flash Free)
   ├── coder #1 (Iceman Contributor Free) ─┐
   ├── coder #2 ───────────────────────────┤  périmètres de fichiers disjoints
   ├── coder #3 ───────────────────────────┤  (4 maximum)
   └── coder #4 ───────────────────────────┘
   │  vérifie : git status, diff, build, tests
   │  sauvegarde avant toute réécriture
   ▼
commit local ──(accord explicite)──> push
```

Les codeurs travaillent dans le même arbre de travail, mais **jamais sur les
mêmes fichiers**. Ils ne commitent pas et ne poussent pas : l'orchestrateur
relit, vérifie et intègre.

## 3. Rôles et modèles

| Rôle | Agent | Mode | Modèle | Responsabilités |
|---|---|---|---|---|
| Orchestrateur | `orchestrator` | primaire | `opencode/deepseek-v4.1-flash-free` | Cadrer, découper, déléguer, vérifier, intégrer, committer, documenter |
| Codeur | `coder` | sous-agent | `opencode-go/iceman-contributor-free` | Exécuter une tâche bornée, sauvegarder, tester, rapporter |

L'orchestrateur ne peut lancer que l'agent `coder` (`subagent` : `*` refusé,
`coder` autorisé). Les codeurs ne peuvent lancer aucun sous-agent.

## 4. Fichiers de l'architecture

| Chemin | Rôle |
|---|---|
| `opencode.jsonc` | Configuration projet : modèle par défaut, agent par défaut, permissions globales, snapshots, watcher |
| `.opencode/agents/orchestrator.md` | Agent primaire : prompt, modèle, permissions |
| `.opencode/agents/coder.md` | Sous-agent : prompt, modèle, permissions |
| `.opencode/commands/orchestrate.md` | Commande `/orchestrate <objectif>` |
| `.opencode/scripts/backup.sh` | Sauvegarde horodatée avec manifeste SHA-256 |
| `AGENTS.md` | Règles projet chargées pour tous les agents |
| `docs/ORCHESTRATION.md` | Ce document |

La configuration globale `~/.config/opencode/` n'est pas modifiée : tout est
porté par la configuration projet.

## 5. Flux de travail

1. **Cadrage.** L'orchestrateur lit `AGENTS.md`, `docs/POINT_SITUATION.md`,
   `docs/ROADMAP.md`, `docs/ORCHESTRATION.md` et les décisions pertinentes.
2. **Découpage.** Il produit une liste de tâches avec, pour chacune :
   objectif, fichiers autorisés, fichiers interdits, critères d'acceptation,
   commandes de validation.
3. **Vérification des périmètres.** Il s'assure que deux tâches parallèles ne
   partagent aucun fichier. Les fichiers transverses (`CMakeLists.txt`,
   `DECISIONS.md`, `docs/POINT_SITUATION.md`) sont réservés à l'orchestrateur
   ou sérialisés.
4. **Sauvegarde.** Avant toute réécriture, l'orchestrateur ou le codeur exécute
   `.opencode/scripts/backup.sh` sur les fichiers existants concernés.
5. **Exécution.** L'orchestrateur lance jusqu'à quatre `coder` en parallèle.
   Chaque prompt contient le contexte complet : les sous-agents démarrent avec
   un contexte vierge.
6. **Vérification.** Chaque rapport est contrôlé : `git status --short`,
   `git diff`, build, tests. Un codeur qui annonce « terminé » n'est pas cru
   sur parole.
7. **Intégration.** L'orchestrateur corrige ou re-scope si nécessaire, met à
   jour la documentation, puis commit.
8. **Publication.** `git push` demande une confirmation explicite. Jamais de
   `--force`.

## 6. Règles de parallélisation

- **Maximum 4 codeurs** simultanés.
- **Périmètres disjoints** : un fichier ne peut être édité que par un seul
  codeur à la fois.
- **Répertoires de build séparés** : `build-coder-1`, `build-coder-2`, etc.
  (`build*/` est ignoré par Git). Ne pas partager `build/` entre codeurs.
- **Pas de dépendance cachée** : si la tâche B dépend du résultat de A, lancer
  A, vérifier, puis lancer B.
- **Fichiers partagés** : les réserver à l'orchestrateur, ou les traiter en
  série.
- **Taille des tâches** : une tâche doit tenir dans un rapport court et produire
  un diff relisible. Découper les chantiers trop larges.

Exemple de découpage pour un correctif runtime :

| Codeur | Périmètre |
|---|---|
| 1 | `runtime/src/rt_win.cpp`, `runtime/include/rt_win.h` |
| 2 | `runtime/src/rt_d3d9.cpp`, `runtime/src/rt_d3d9_gen.cpp` |
| 3 | `tools/recomp/ct3rc.py`, `tests/` associés |
| 4 | `docs/` uniquement (aucun code) |

## 7. Sauvegardes

### Création

```sh
.opencode/scripts/backup.sh docs/HANDOFF.md runtime/src/rt_win.cpp
```

Chaque appel crée :

```text
/tmp/opencode/ct3-backups/<horodatage UTC>-<pid>/
├── manifest.tsv
├── README.txt
└── <arborescence relative au dépôt ou external/...>
```

`manifest.tsv` contient : `source`, `backup`, `sha256`, `bytes`, `mode`. Le
script vérifie l'empreinte de chaque copie et échoue si elle diffère. Il ne
modifie jamais la source et n'utilise ni `rm`, ni `mv`, ni `truncate`, ni
`sed -i`.

### Restauration

1. Lire `manifest.tsv` et vérifier l'empreinte de la copie.
2. Sauvegarder l'état courant avant de le remplacer :
   `.opencode/scripts/backup.sh <fichier>`.
3. Recopier manuellement : `cp -a -- <chemin-backup> <chemin-source>`.
4. Vérifier le résultat (`git diff`, build, tests).

Ne jamais supprimer la source avant d'avoir vérifié la copie.

## 8. Permissions

Les règles sont ordonnées ; **la dernière règle qui correspond gagne**. Les
règles globales de `opencode.jsonc` s'appliquent avant les règles d'agent.

### Global (`opencode.jsonc`)

- Refus : `rm`, `mv`, `truncate`, `dd`, `shred`, `sed -i`, `perl -i`,
  `find -delete`, `git reset --hard`, `git clean`, `git checkout --`,
  `git restore`, `git push --force`.
- Demande : `git push`.
- `external_directory` : demande par défaut ; `/tmp/opencode/` et le dossier
  du jeu sont autorisés en lecture.
- Édition refusée sur le dossier du jeu, `CT3.exe` et `CT3.original.exe`.

### Orchestrateur

- Peut lancer uniquement le sous-agent `coder`.
- Peut committer (`git commit`) et utiliser le script de sauvegarde.
- Ne peut pas éditer `.git/`, `dist/` ni `captures/`.

### Codeur

- Aucun sous-agent.
- Aucun `git commit`, `git push`, `git tag`, `git merge`, `git rebase`,
  `git cherry-pick`, `git revert`.
- Ne peut pas éditer `.git/`, `dist/` ni `captures/`.
- Doit sauvegarder avant toute réécriture et rester dans son périmètre.

## 9. Utilisation

### Lancer une orchestration

```text
/orchestrate corriger le pic de mémoire du cache D3D9 et ajouter un test
```

La commande transmet l'objectif à l'agent `orchestrator`, qui applique le flux
de la section 5.

### Recharger la configuration

Après une modification de `opencode.jsonc`, des agents ou des commandes :

```text
/reload
```

Une session existante conserve son agent et son modèle. Pour utiliser
`orchestrator` comme agent par défaut, ouvrir une nouvelle session ou changer
d'agent explicitement.

### Vérifier la découverte

```sh
opencode debug agents
opencode debug config
opencode models | grep -E 'deepseek-v4.1-flash-free|iceman-contributor-free'
```

## 10. Validation de l'architecture

```sh
bash -n .opencode/scripts/backup.sh
python3 -c "import yaml, pathlib; [yaml.safe_load(pathlib.Path(p).read_text().split('---')[1]) for p in ['.opencode/agents/orchestrator.md', '.opencode/agents/coder.md', '.opencode/commands/orchestrate.md']]"
git diff --check
tools/check_no_game_data.sh
```

Pour valider le JSONC, utiliser un analyseur JSONC ou `opencode debug config`
(la configuration est chargée par OpenCode lui-même).

## 11. Dépannage

| Symptôme | Cause probable | Action |
|---|---|---|
| Agent `orchestrator` introuvable | Fichier absent, frontmatter invalide, configuration non rechargée | Vérifier `.opencode/agents/orchestrator.md`, exécuter `/reload`, ouvrir une nouvelle session |
| Modèle indisponible | Identifiant erroné, authentification absente, quota | `opencode models`, vérifier le provider, réessayer |
| Permission refusée | Une règle `deny` correspond, ou la dernière règle l'emporte | Relire l'ordre des règles dans `opencode.jsonc` et l'agent |
| Sauvegarde impossible | Chemin absent, droits insuffisants, nom ambigu | Vérifier le chemin et `CT3_BACKUP_ROOT` |
| Codeur hors périmètre | Prompt incomplet ou tâche mal découpée | Arrêter, re-scoper, relancer |
| Conflit entre codeurs | Périmètres non disjoints | Sérialiser ou redécouper |

## 12. Limites connues

- Pas de fusion automatique : l'orchestrateur intègre manuellement.
- Les sous-agents ont un contexte vierge : un prompt incomplet produit un
  travail incomplet.
- Les modèles gratuits peuvent avoir des quotas ou des indisponibilités
  temporaires ; dans ce cas, sérialiser ou réessayer.
- Aucune validation matérielle Switch n'est possible sur l'hôte actuel :
  devkitA64/libnx ne sont pas installés.
- Les snapshots OpenCode ne remplacent pas les sauvegardes explicites.

## 13. Références

- `AGENTS.md` — règles projet.
- `docs/POINT_SITUATION.md` — état courant.
- `docs/ROADMAP.md` — jalons et priorités.
- `DECISIONS.md` — décisions, notamment D-080 à D-083.
- `docs/HANDOFF.md` — passation et variables runtime.
- `CONTRIBUTING.md` — style de contribution et sign-off.
- `PUBLISHING.md`, `LEGAL.md` — cadre de publication.
