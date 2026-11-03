# TODO : Crazy Taxi 3 sous Linux

Liste de travail, dans l'ordre de priorité indiqué par l'utilisateur. Le détail technique de chaque point est dans
`DECISIONS.md` ; l'état d'ensemble est dans `docs/HANDOFF.md`. Mise à jour : 3 nov. 2026.

## Terminé ou en place

- **Démarrage lent (écran noir)** : premier affichage à environ 9 à 10 s (contre 43 à 45 s avant les portages), sur
  l'écran de l'utilisateur comme en Xvfb. Routines portées en code natif, vérifiées bit à bit contre la traduction
  (`CT3_FILTER_CHECK`, `CT3_FILTER_CHECK_DITHER`) : filtre `FUN_004DACC6` (mode 1), tramage `FUN_004DDB6F` et
  `FUN_004DD45C`, alpha DXT3 `FUN_004F0D8B`, ajustement des extrémités `FUN_004EFA9B`. Voir `docs/PORTAGE_FILTRE.md`
  et D-068 à D-070.
- **Écran de chargement vidéo (D-070)** : `avi/loading.avi` est lue au lancement (FFmpeg, build LGPL), en plein écran,
  fond noir, vidéo à sa taille d'origine et centrée. La fenêtre du jeu reste cachée pendant la vidéo et s'affiche au
  premier affichage du jeu. Vérifié sur l'écran de l'utilisateur.
- **Manettes** : premier jet fait (D-064, D-065), testé avec une DualShock 4.
- **Son** : premier jet fait (D-067). Bruitages et musique fonctionnent.

## 1. Manettes : reste à faire

- Vérifier les touches de conduite en jeu (klaxon, Crazy Dash) et la touche de validation des menus (Start ou L1).
- Corriger l'effet d'un appui qui semble déplacer le curseur de deux entrées.
- v1.1 : modifier les paramètres des manettes dans l'interface du jeu (et écrire `Config.ini`).
- Hors liste : étendre `Config.ini` (résolution, autres réglages).

## 2. Son : reste à faire

- Atténuation 3D des tampons (absente : les méthodes 3D n'ont pas d'effet, `rt_extra.cpp`).
- Interpolation du rééchantillonnage (plus proche voisin actuellement).
- Cinématiques : muettes, faute de flux DirectShow (D-061).

## 3. Ralentissements en jeu (compilation des shaders) : à traiter

- Constat : des à-coups en jeu, aléatoires. Non mesurés. La compilation des shaders et des pipelines au premier usage
  est la piste principale.
- Pistes :
  - Vérifier que le cache DXVK (`~/.cache/dxvk/*.dxvk.bin`) est écrit et relu d'un lancement à l'autre.
  - Compilation asynchrone des pipelines (option DXVK, D-013).
  - Préchauffage pendant l'écran titre, ou précompilation à partir du cache.
- À mesurer d'abord : temps passé dans la compilation (journal DXVK ou `perf`).

## 4. Démarrage : reste à faire

- Le bloc de couleur DXT3 `FUN_004F0019` (chemin `param_2 == 0`) est encore traduit. Sa structure est décrite dans
  `docs/PORTAGE_FILTRE.md`, section 11. Gain attendu plus modeste que les précédents.
- Les autres modes du filtre (2 et 3) ne sont pas portés (non vérifiés). Le décodeur DXT1 natif reste désactivé.
- Après le premier affichage, le jeu montre encore son logo en fondu (images sombres vers les images 260 et 500 à 600,
  mesure Xvfb). Couvrir ces fondus par la vidéo est une décision à prendre.

## 5. Win32 : manques et implémentations partielles

- Sur 203 imports de `CT3.exe`, 189 sont implémentés. 14 manquent : `DialogBoxParamA`, `EndDialog`, `GetDlgItem`,
  `IsDlgButtonChecked`, `CheckRadioButton`, `RaiseException`, `RtlUnwind`, `CreateFileW`, `EnumSystemLocalesA`,
  `GetLocaleInfoW`, `GetEnvironmentStrings`, `GetIconInfo`, et deux ordinaux (`COMCTL32 #17`, `DSOUND #11`).
  Un appel à l'un d'eux arrête le runtime (code 3). Aucun n'apparaît dans nos journaux, mais leurs sites d'appel ne
  sont pas vérifiés.
- Implémentations partielles : `GetStringType` (type 1 seulement), `LCMapString` (indicateurs 0x100 et 0x200
  seulement).
- À faire : vérifier les sites d'appel des 14 imports (le p-code) ; commencer par `CreateFileW`, `RaiseException` et
  `RtlUnwind`.
- Licence : le dépôt n'a pas encore de licence. Tant que ce n'est pas tranché, aucun code de Wine ne peut être copié.
  Une réimplémentation à partir de la documentation et des tests de Wine reste possible.

## 6. Cinématiques

- Seule `avi/loading.avi` est présente dans la copie (D-004, D-005). Les cinématiques `sa_ic.avi`, `go_bb.avi` et
  `go_an.avi` manquent : sans elles, le graphe DirectShow factice (D-061) termine la lecture immédiatement.
- À faire quand les fichiers seront disponibles : identifier leur codec avec `ffprobe` et vérifier la lecture avec
  FFmpeg (déjà compilé dans le préfixe, `tools/deps/build_ffmpeg.sh`).

## Constats à traiter (hors liste de l'utilisateur)

### Plantage A (D-069) : faute à 0x3F800000 au bout de 5 à 12 min de course

- Revenu sur le build avec portages natifs (3 nov., 301 s de course), même signature : `EBX = EBP = 0x45183AD4`,
  fonction `FUN_00414ED0`. Le plantage précède les portages, mais rien n'exclut une écriture native.
- Écrivain de la valeur fautive : non identifié.
- À faire : si reproductible, comparer avec `CT3_FILTER_NATIVE=0`.

### Plantage à la sortie du jeu (adresse non traduite 0x005154b5)

- En quittant le jeu, le journal affiche `appel vers une adresse non traduite 0x005154b5`. Fonction de la table
  `atexit`, non désassemblée.
- Impact : seulement la sortie. Sans effet sur le jeu en cours.
- À faire : repérer les écritures de pointeurs de fonction dans `atexit` / `_onexit`.

### Intros d'introduction : encore buggées

- Constat utilisateur (2 nov.) : les cinématiques d'introduction restent défectueuses. Quelle intro, quel moment, quel
  défaut : à relever avec une capture au moment du bug (`CT3_SHOT`).
- Lien possible avec le graphe DirectShow factice (D-061).

### Personnages déformés dans les cinématiques

- Constat utilisateur : polygones manquants ou décalés pour les personnages (LV et SA), décor et véhicules corrects.
- Piste (non vérifiée) : palette de matrices de squelette (`VERTEXBLEND`), calcul de matrices `FUN_004E7813`.
- À faire : capture au moment du défaut, puis comparaison.

### « Tas invité épuisé » après environ 20 minutes

- Un run s'est arrêté à 17:45 par `rt: tas invité épuisé`.
- À faire : `CT3_ALLOCLOG=1` et `CT3_HEAPCHK=1` sur une session longue, pour trouver la fuite.
