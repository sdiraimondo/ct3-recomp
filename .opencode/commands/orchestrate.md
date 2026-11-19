---
description: "Lance une orchestration multi-codeurs sur un objectif"
agent: orchestrator
---

Objectif : $ARGUMENTS

Applique la procédure de `docs/ORCHESTRATION.md` :

1. Lis `AGENTS.md`, `docs/POINT_SITUATION.md`, `docs/ROADMAP.md`,
   `docs/ORCHESTRATION.md` et les décisions pertinentes.
2. Découpe l'objectif en tâches indépendantes avec des périmètres de fichiers
   disjoints. Réserve les fichiers transverses à l'orchestrateur.
3. Lance au plus 4 codeurs `coder` en parallèle. Pour chacun, fournis le
   contexte complet, les fichiers autorisés et interdits, l'obligation de
   sauvegarde, les commandes de validation et le format de rapport.
4. Vérifie chaque rapport (`git status`, diff, build, tests), sauvegarde avant
   toute réécriture, puis intègre.
5. Commit après vérification complète (`git commit -s`, message en français).
   Demande confirmation avant tout `git push`.
6. Termine par un rapport concis : réalisé, vérifié, bloqué, prochaine étape.

Si l'objectif est vide ou ambigu, demande une clarification avant de lancer
des codeurs.
