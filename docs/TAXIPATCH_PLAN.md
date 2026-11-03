# taxipatch : analyse et plan d'intégration

Analyse du 3 nov. 2026 (`taxipatch` v0.3.0, GPL-3.0 ; `ct3tweaks`, Apache-2.0, en fin de vie). Aucun code de ces
projets n'est copié dans le dépôt : ce document ne contient que des faits (adresses, constantes, comportements).

## 1. Ce que fait taxipatch

Plugin Windows (`taxipatch.asi`, chargé par Ultimate ASI Loader). Il accroche des fonctions du jeu par décalage
relatif à l'image (adresse virtuelle = `0x400000 + décalage`), lit `taxipatch.ini`, et modifie des globales en mémoire.

| Fonction | Décalage | Adresse (notre traduction) | Effet |
|---|---|---|---|
| `FrameLimiter::update` | `0x7D00` | `FUN_00407D00` | `adaptive_mode = false` (60 FPS) |
| `BootLogoSequenceUpdate` | `0x2E160` | `FUN_0042E160` | compteur des logos à 1024 (saut des logos) |
| `Camera::UpdateFromGlobals` | `0x31C80` | `FUN_00431C80` | FOV et format selon la fenêtre |
| `Camera::Init` | `0x316A0` | `FUN_004316A0` | constantes de perspective |
| `Map::UpdateDrawDistance` | `0x47ED0` | `FUN_00447ED0` | distances de rendu de la carte (×2 par défaut) |
| `SpriteRenderQueue::Flush` | `0x24990` | `FUN_00424990` | réinitialise `D3DTSS_TEXCOORDINDEX` (correctif UI) |
| `CStreamHandler::WorkerThread` | `0x134D0` | `FUN_004134D0` | boucle de nettoyage des flux (sonore) |
| `Effects::DrawDestinationBorders` / `DrawParticles` | `0x74520` / `0x62FA0` | `FUN_00474520` / `FUN_00462FA0` | report des dessins d'effets après la caméra |
| `CD3DApplication::InitWindow` / `WndProc` / `BuildPresentParams` / `CreateWindowExA` | `0x28DA0`, `0x29010`, `0x283D0` | `FUN_00428DA0`, `FUN_00429010`, `FUN_004283D0` | mode fenêtré, sans bordure, résolution, rafraîchissement |

Toutes ces fonctions existent à ces adresses dans notre `CT3.exe` (vérifié : présence du code à chaque adresse et
structure du `FrameLimiter` : `limiter_enabled` en `0x2C`, `adaptive_mode` en `0x2D`, `sleep_before_frame` en `0x2E`).

## 2. Ce que CT3Tweaks a déjà modifié dans notre `CT3.exe`

Comparaison octet par octet avec `CT3.original.exe` : **12 octets différents**, dans 4 plages.

- `0x407A83` et `0x407A8D` : résolution interne 640×480 → **1280×720**.
- `0x407E4B` : seuil du limiteur de trames 30 → **60** (fonctionnalité « 60 FPS »).
- `0x431764`, `0x46ADC9` : constante d'aspect 4/3 → **16/9**.
- `0x46ADD0` : FOV 60° → **80°**.

Conséquences :

1. Le `CT3.exe` utilisé pour nos tests et notre binaire est **modifié** (par l'utilisateur, avec CT3Tweaks, le 28 déc. 2025).
   Notre règle (ne jamais modifier `CT3.exe`) est respectée par le projet, mais la base de travail n'est pas l'original.
2. `CT3.original.exe` a l'empreinte Xplosiv acceptée par taxipatch (`9ad9ad77…`). Notre `CT3.exe` a une autre empreinte
   (`9d2d1e76…`), donc taxipatch le refuserait tel quel.
3. Les correctifs de bugs et le saut des logos n'étaient pas dans CT3Tweaks : ils sont absents aujourd'hui.

## 3. Ce qui est transposable dans notre architecture

- **Fichier de configuration** : `Config.ini` est déjà lu par le runtime (manettes, D-065). Ajouter `[game]`, `[distance]`,
  `[patches]` est direct.
- **Globales** : ce sont des adresses dans notre mémoire invitée (même base `0x400000`). Lecture et écriture directes.
- **Accroches de fonctions** : même mécanisme que `NATIVE_OVERRIDES` (`tools/recomp/ct3rc.py`), étendu à des accroches
  « avant », « enchaînée » (appel de la traduction après la modification) et « remplacement ». Les appels directs et
  indirects passent par `dispatch`, donc toutes les accroches couvrent les deux.
- **Fenêtre et présentation** : `runtime/src/rt_win.cpp` (style de fenêtre, sans bordure) et la création du device
  (`BuildPresentParams`) sont sous notre contrôle.

## 4. Ce qui est plus difficile

- **Effets (`effects.rs`)** : report des dessins et reconstruction de la vue. Logique d'ordre fin : à porter en dernier, avec
  des captures avant/après.
- **Mode fenêtré et sans bordure** : les tailles de fenêtre changent la mise en page de l'interface ; à vérifier à l'écran.
- **60 FPS** : désactiver `adaptive_mode` ne suffit pas à lui seul à expliquer le seuil 60 de CT3Tweaks ; à mesurer
  (présentations par seconde) avant de conclure. Plus de fréquence = plus de charge, donc les à-coups (point 3 du TODO)
  deviendront plus visibles.

## 5. Licence

- taxipatch : **GPL-3.0**. Copier son code impose une licence compatible GPL-3.0 pour le projet.
- ct3tweaks : **Apache-2.0**, en fin de vie, et il **modifie le fichier `CT3.exe`** : exclu.
- Le dépôt n'a toujours pas de licence. Une réécriture à partir des faits ci-dessus (clean-room) reste possible sans
  licence préalable, mais doit rester fidèle au comportement, pas au code.

## 6. Plan proposé

**Phase 0 : décisions (utilisateur)**
- Base de travail : `CT3.original.exe` (recommandé) plutôt que le `CT3.exe` modifié par CT3Tweaks. Les réglages
  sont alors reproduits par le runtime et `Config.ini`, sans modifier de fichier du jeu.
- Licence du projet (GPL-3.0 si on réutilise du code taxipatch).

**Phase 1 : configuration** : `Config.ini` (`[game]` résolution, mode d'affichage, FOV, MSAA ; `[distance]` ; `[patches]`
saut des logos, 60 FPS ; `[general]`). Valeurs par défaut identiques à taxipatch.

**Phase 2 : fonctionnalités simples** (accroches et globales)
1. Saut des logos (`BootLogoSequenceUpdate`) : supprime aussi les fondus du logo, point laissé ouvert après le démarrage.
2. FOV et format (`Camera::Init`, `UpdateFromGlobals`).
3. Résolution, mode fenêtré et sans bordure (`BuildPresentParams`, `InitWindow`, `CreateWindowExA`).
4. 60 FPS (`FrameLimiter::update`), vérifié par mesure de présentations par seconde.

**Phase 3 : correctifs de bugs** (les points qui vous intéressent)
1. Textures d'interface (`Flush` : réinitialisation de `TEXCOORDINDEX`). Le plus simple et le plus sûr.
2. Boucle des flux sonores (`CStreamHandler::WorkerThread`).
3. Distances de la carte (`Map::UpdateDrawDistance`).
4. Effets (`DrawDestinationBorders`, `DrawParticles`) : en dernier, avec captures.

**Phase 4 : menu Options intégré** : étude de l'écran d'options (état de la machine, tables de textes, saisie),
puis ajout des entrées et enregistrement dans `Config.ini`. C'est la partie la plus incertaine.

**Vérification à chaque phase** : captures avant/après (`CT3_SHOT`), présentations par seconde (`CT3_PRESENT_LOG`), tests
obligatoires, et un test de chargement de `Config.ini`.
