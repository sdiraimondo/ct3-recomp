# TODO : Crazy Taxi 3 sous Linux

Liste de travail, dans l'ordre de priorité indiqué par l'utilisateur (2 nov. 2026). Le détail technique
de chaque point est dans `DECISIONS.md` ; l'état d'ensemble est dans `docs/HANDOFF.md`.

## 1. Manettes via SDL (tests plus rapides à la manette) : premier jet fait (D-064)

- Fait : boutons et stick gauche traduits en touches DIK (menus validés avec une manette virtuelle).
- Mapping PS2 et `Config.ini` : voir D-065. Testé avec une DualShock 4 (boutons et stick) par l'utilisateur.
- Reste : la vérification complète des touches de conduite en jeu (klaxon, Crazy Dash), la touche de
  validation des menus (Start ou L1), et l'effet d'un appui qui semble déplacer le curseur de deux entrées.

- Objectif : piloter le jeu à la manette (menus et conduite) pour accélérer les tests.
- Point de départ : le clavier passe déjà par DirectInput (D-046) et les événements SDL. `EnumDevices` ne
  renvoie que le clavier (`GUID_SysKeyboard`) ; `CreateDevice`/`GetDeviceState` ne couvrent pas les joysticks.
- À faire :
  - Vérifier quels périphériques le jeu énumère (`GUID_Joystick`, ou XInput : `XInput1_3.dll` est présent dans
    le dossier du jeu, à contrôler dans l'import).
  - Remplir `DIJOYSTATE` (axes, boutons, POV) depuis SDL (GameController ou Joystick).
  - Table de correspondance boutons et axes du jeu vers les contrôles SDL, à valider sur une vraie manette.

### Hors liste : paramètres dans Config.ini et UI (v1.1)

- Fait : le mapping des manettes est lu dans `Config.ini` (D-065).
- À faire : étendre `Config.ini` (résolution, autres réglages du jeu).
- v1.1 : modifier les paramètres des manettes directement dans l'interface du jeu (et écrire `Config.ini`).

## 2. Son (totalement absent) : premier jet fait (D-067)

- Fait : mixage des tampons DirectSound vers SDL, avec format, fréquence, volume et panoramique.
- Confirmé par l'utilisateur : les bruitages fonctionnent, et la musique est jouée en jeu.
- Reste : les vidéos (cinématiques) restent muettes, faute de flux DirectShow (D-061) ; atténuation 3D ;
  interpolation du rééchantillonnage.

- Constat : DirectSound est un stub silencieux (D-038). Le curseur de lecture suit l'horloge (D-051), et les
  banques `Media/SE/*.bin` et les fichiers `.aif`, `.ogg` sont lus mais rien n'est joué.
- Plan prévu : sortie SDL audio (D-008). Le jeu écrit dans des tampons verrouillés (`Lock`/`Unlock`, deux zones
  au bouclage, D-058) : il faut mélanger ces tampons vers SDL à la position de lecture réelle.
- À faire : un fil de sortie SDL qui lit les tampons en lecture (`Play`, `Stop`, `SetCurrentPosition`) ; ensuite
  le décodage des formats du jeu (`xbhm`, AIFF, Ogg) et la musique.
- Attention : la cinématique et le titre dépendent du curseur d'écriture du décodeur (D-057) ; ne pas casser
  le bouclage.

## 3. Compilation des shaders (ralentissements en jeu)

- Constat : des à-coups en jeu, dus à la compilation des shaders et des pipelines à leur premier usage.
- Pistes :
  - Le cache d'état DXVK est déjà utilisé (`~/.cache/dxvk/*.dxvk.bin`, 222 shaders au lancement). Vérifier qu'il
    est bien écrit et relu d'un lancement à l'autre.
  - Compilation asynchrone des pipelines (option DXVK, à vérifier dans la version compilée, D-013).
  - Préchauffage pendant l'écran titre, ou précompilation à partir du cache.
- À mesurer d'abord : temps passé dans la compilation (journal DXVK ou `perf`), pour cibler la piste.

## Constats à traiter (hors liste de l'utilisateur)

### Cinématiques d'introduction (intros) : encore buggées

- Constat utilisateur (2 nov. 2026) : les cinématiques d'introduction restent défectueuses. À investiguer
  (détail à relever : quelle intro, quel moment, quel défaut ; capture au moment du bug).
- Lien possible : les vidéos sont remplacées par le graphe DirectShow factice (D-061) ; la fin immédiate de la
  lecture peut changer le déroulement des intros. À vérifier avant de conclure.

### Démarrage lent : écran noir pendant une bonne minute

- Constat utilisateur (2 nov. 2026) : le jeu ne démarre pas avant une bonne minute, écran noir.
- Mesuré (D-068) : premier `Present` à environ 79 s, puis 43 s après le correctif x87 (`truncl` retiré).
- Reste environ 40 s de calcul au chargement : traitement des maillages (`FUN_004DACC6` et appelants). Suite :
  remplacement natif ciblé après vérification, ou optimisation générale du code traduit.
- Piste notée : le thread audio du jeu acquitte les demandes d'arrêt des sons avec une période de 250 ms
  (`0x413350`, `0x412AB0`, `0x4134D0`).
- Diagnostic : `CT3_PRESENT_LOG=1`.

### Plantage à la sortie du jeu (adresse non traduite 0x005154b5)

- Constat : en quittant le jeu (menu « Exit » ou fermeture), le journal affiche `rt: appel vers une adresse non
  traduite 0x005154b5 (retour invité 0x00517930)`. Le trampoline `0x53aefb` est désormais suivi (commit `8c3ee82`),
  mais `0x5154b5` est une fonction pointée par la table `atexit` (`0x7DB7D8`), remplie à l'exécution. Ghidra ne l'a
  pas désassemblée (la zone `0x5154a0`-`0x5154be` n'est pas dans l'export).
- Impact : seulement la sortie (pas de nettoyage, code de sortie 3). Sans effet sur le jeu en cours.
- À faire : repérer les écritures de pointeurs de fonction dans `atexit` / `_onexit`, et exporter les fonctions
  référencées par constante dans le code (passe `pcode-all-ptrs`).

### Personnages déformés dans les cinématiques

- Constat utilisateur : dans les cinématiques LV et SA, des polygones des personnages manquent ou sont décalés.
  Le décor et les véhicules sont corrects, et le même personnage est correct en jeu.
- Piste (non vérifiée) : les personnages sont peut-être déformés par des matrices de squelette (palette de
  matrices, `VERTEXBLEND`). Le calcul de matrices 4×4 passe par `FUN_004E7813`, qui apparaît dans les piles de
  rendu de scène. Une erreur de précision dans le chemin x87 ou dans les indices de matrices serait à vérifier.
- À faire : capture d'écran au moment du défaut (`CT3_SHOT`), puis comparaison avec le matériel des personnages.

### Arrêt « tas invité épuisé » après environ 20 minutes

- Constat : le run lancé à 17:26 s'est arrêté à 17:45 par `rt: tas invité épuisé`. Un autre run, sans rapport
  apparent, avait déjà présenté ce message en fin de cinématique SA (avant le correctif D-061).
- À faire : `CT3_ALLOCLOG=1` et `CT3_HEAPCHK=1` sur une session longue, pour identifier la fuite (appelant
  dominant). Les allocations du graphe DirectShow factice sont minimes (D-061) : vérifier qu'elles ne contribuent
  pas.
