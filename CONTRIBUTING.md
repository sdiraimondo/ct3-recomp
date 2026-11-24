# Contribuer

Merci de votre intérêt pour `ct3-linux`. Le projet est un runtime et un
recompilateur ; il ne contient pas le jeu et ne doit jamais en contenir de
copie.

## Licence des contributions

En proposant une contribution, vous acceptez qu'elle soit distribuée sous
**GPL-3.0-or-later**, la licence du projet. Vous certifiez également le
[Developer Certificate of Origin 1.1](https://developercertificate.org/) en
ajoutant une ligne `Signed-off-by` à chaque commit :

```sh
git commit -s -m "runtime: corriger ..."
```

La ligne doit correspondre à votre nom et à votre adresse e-mail :

```
Signed-off-by: Prénom Nom <prenom.nom@example.org>
```

N'ajoutez pas de `Signed-off-by` pour quelqu'un d'autre. Si vous contribuez
pour le compte d'un employeur, vérifiez que vous avez le droit de le faire.

## Ce qu'il ne faut jamais joindre

- `CT3.exe`, `CT3.original.exe`, DLL, exécutables Windows ou copies du jeu ;
- les assets (`avi/`, `Media/`, textures, sons, polices, etc.) ;
- le pack `ct3_data.bin`, `ct3_image.h`, `ct3_image.json` ;
- le C généré (`gen_funcs*.c`, `gen_dispatch.c`, `pcode.json`) et
  `libct3gen.a` ;
- les captures d'écran, vidéos ou journaux contenant des assets du jeu ;
- les clés, mots de passe, jetons ou liens vers des copies non autorisées.

Ces éléments sont des dérivés du jeu. Une pull request qui en contient sera
fermée. Le script `tools/check_no_game_data.sh` vérifie l'arbre suivi et
l'historique ; lancez-le avant de proposer une modification.

## Construire et tester

Voir `README.md` (« Building and running »). En résumé :

```sh
cmake -S . -B build -G Ninja \
    -DCT3_EXE="/chemin/vers/Crazy Taxi 3/CT3.exe" \
    -DCT3_DEPS_PREFIX="$HOME/.local/ct3-deps/prefix"
cmake --build build --target ct3_run
```

Les tests qui n'ont pas besoin du jeu (`test_cpu`, `test_vec`) doivent rester
verts. Les tests traduits (`test_x87`, `test_g2`, `ct3_run`) nécessitent votre
copie locale et ne sont pas exécutés dans la CI publique.

Sans `CT3_EXE`, la configuration doit rester valide : les cibles qui dépendent
du jeu (`ct3gen`, `ct3pack`, `ct3_run`, `test_g2`, `test_x87`) sont alors
ignorées ou échouent avec un message explicite.

## Code

- C11 pour le runtime C, C++17 pour les ponts D3D9 ; respectez le style du
  fichier voisin.
- Pas de dépendance nouvelle sans discussion préalable : elle doit être
  compatible avec GPL-3.0-or-later, documentée dans `THIRD_PARTY.md` et, si
  elle est distribuée, accompagnée de sa licence dans `LICENSES/`.
- Pas de code Wine copié sans décision explicite : une réimplémentation à
  partir de la documentation et des tests est préférée. Si du code LGPL est
  introduit, il reste sous sa licence et doit être signalé.
- Évitez les adresses ou constantes spécifiques au jeu codées en dur quand
  elles peuvent être lues depuis le pack ou un fichier de configuration.
- Gardez les commits petits et descriptifs ; le dépôt est en français, mais
  l'anglais est accepté.

## Tests et CI

La CI publique vérifie :

- l'absence de données de jeu dans l'arbre suivi et l'historique ;
- la présence de `LICENSE`, `THIRD_PARTY.md`, `LICENSES/` et des documents
  légaux ;
- la syntaxe Python des outils ;
- le build sans jeu et les tests `test_cpu`, `test_vec` et G1 sous Xvfb/llvmpipe.

Le workflow construit dxvk-native depuis le commit documenté dans
`DECISIONS.md` et ne télécharge jamais le jeu. La CI ne doit jamais avoir
besoin de `CT3.exe`. Les tests qui dépendent du pack du jeu restent
conditionnels.

## Signaler un problème

- Bogue ou question : ouvrez une issue en décrivant le comportement attendu et
  observé, sans joindre de fichier du jeu.
- Faille de sécurité : n'ouvrez pas d'issue publique ; utilisez l'onglet
  « Security » du dépôt (private vulnerability reporting) ou contactez le
  mainteneur.
- Contenu sous droits : voir `LEGAL.md`.

## Pull requests

1. Forkez le dépôt et créez une branche.
2. Ajoutez ou mettez à jour les tests si nécessaire.
3. Lancez `tools/check_no_game_data.sh` et les tests disponibles.
4. Signez vos commits (`git commit -s`).
5. Décrivez le changement, les tests effectués et les limites connues.

Merci de garder les pull requests focalisées : un correctif ou une
fonctionnalité par pull request.
