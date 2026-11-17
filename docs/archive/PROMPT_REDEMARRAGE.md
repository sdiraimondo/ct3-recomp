# Prompt pour redémarrage propre

> **Obsolète** — archivé le 17 novembre 2026. Le prompt ci-dessous décrit un état antérieur du
> projet. État courant : `README.md`, `docs/TODO.md` et `docs/SWITCH_PORT.md`.

Copie-colle ce prompt dans un nouveau contexte/session pour reprendre le travail proprement.

```
Tu travailles sur le projet Crazy Taxi 3 natif Linux dans /var/home/stephane/Dev/ct3-linux.

OBJECTIF : Faire tourner Crazy Taxi 3 nativement sous Linux x86_64 par recompilation statique (x86 vers C).

CONTRAINTES ABSOLUES :
- Ne JAMAIS modifier CT3.exe ni CT3.original.exe (lecture seule)
- Ne pas versionner les artefacts dérivés du jeu (pack, C généré, libct3gen.a)
- Le pack doit préserver adresses virtuelles, imports, point d'entrée et BSS
- CT3.exe n'est nécessaire qu'au build, pas au runtime
- Les assets sont trouvés sans dériver du chemin de CT3.exe

ÉTAT ACTUEL :
- Décompilation complète : 5747 fonctions traduites, 309 atteignables sans site non pris en charge
- Pack + runtime fonctionnels (lot 1 et 2 faits, commit 9804d85)
- Jeu démarre mais CRASH juste après le premier écran de chargement avec : "rt: dernière instruction invitée 0x00000000 / faute mémoire (signal 11) à l'adresse invitée 0xc"
- Tests cpu/vec/g2/x87/g1_d3d9_triangle passent
- Build fonctionnel

PROBLÈME À RÉSOUDRE EN PRIORITÉ :
Le crash juste après l'écran de chargement. Il s'agit d'un déférencement de pointeur null (accès offset 0xc). 
Regarder les logs dans /tmp/ct3.log pour plus de détails.
La stack trace montre : 0x0053978b (FUN_0053978b -> ___CxxFrameHandler), 0x0049dd2d, 0x0047a738, etc.

FICHIERS À CONSULTER EN PREMIER :
- RAPPORT_SITUATION.md (rapport détaillé de la session)
- docs/TODO.md
- docs/HANDOFF.md
- DECISIONS.md
- runtime/src/rt_diag.c, runtime/src/rt_shims.c
- build/g3/pcode_all.json pour analyser le crash

MODIFICATIONS EN COURS (non commités) :
- runtime/src/rt_diag.c : ajout de symboles de compatibilité (rt_cur_insn, rt_cs_depth, rt_cs_stack, etc.)
- runtime/src/rt_shims.c : ajouts de stubs (RaiseException, RtlUnwind, CreateFileW, DSOUND #11)
- Autres fichiers runtime modifiés

COMMANDES UTILES :
```bash
cd /var/home/stephane/Dev/ct3-linux
git status --short
git log --oneline -3
cmake --build build --target ct3_run
cd "/run/media/stephane/BC250_USB/Games/Crazy Taxi 3" && DISPLAY=:0 timeout 60 /var/home/stephane/Dev/ct3-linux/build/tests/run/ct3_run .
tail -80 /tmp/ct3.log
```

APPROCHE :
1. Lire RAPPORT_SITUATION.md pour comprendre l'historique
2. Analyser le crash (stack trace, fonctions impliquées)
3. Déboguer méthodiquement - possiblement utiliser le build de traçage (build-o0)
4. Corriger le crash sans casser ce qui fonctionne
5. Une fois stable, continuer avec la roadmap (stabilité session longue, shaders, cinématiques, etc.)

Ne pas faire de changements drastiques sans comprendre. Tester à chaque étape.
```
