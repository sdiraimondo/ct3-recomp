# Limites juridiques

> Ce document décrit les règles que le projet s'impose. Ce n'est **pas un avis
> juridique** : pour publier, distribuer ou modifier le jeu, consultez un
> avocat compétent dans votre juridiction.

## Le projet et le jeu

`ct3-linux` est un projet indépendant de recompilation : il fournit un runtime
Win32, un recompilateur et des outils. Le dépôt ne contient **aucun code du
jeu, aucun asset, aucune clé et aucun binaire dérivé**.

Pour utiliser le projet, vous devez posséder une **copie légale** de
*Crazy Taxi 3* pour PC. Le recompilateur lit votre `CT3.exe` et vos assets
localement ; il ne les télécharge pas et ne les inclut pas.

## Ce qui est dérivé du jeu

Les artefacts suivants sont des **œuvres dérivées** de votre copie du jeu :

- le C traduit produit par `tools/recomp/ct3rc.py` ;
- `libct3gen.a` et tout binaire qui le lie (par exemple `ct3_run`) ;
- le pack `ct3_data.bin` / `ct3_image.h` / `ct3_image.json` (en-têtes PE,
  `.text`, `.rdata`, `.data`, `.rsrc`) ;
- les captures, vidéos ou journaux contenant des assets du jeu.

Ils peuvent être générés et utilisés **localement**, sur votre propre copie.
Ils ne doivent **jamais** être versionnés, publiés, téléversés, vendus ou
partagés. Le fichier `.gitignore` et `tools/check_no_game_data.sh` protègent le
dépôt contre un ajout accidentel ; ils ne remplacent pas votre responsabilité.

## Licence du projet

Le code original du dépôt est sous **GPL-3.0-or-later** (voir `LICENSE`).
Cette licence ne couvre que le code du projet. Elle ne vous donne **aucun
droit** sur *Crazy Taxi 3*, sur ses assets, sur les marques de SEGA ou sur les
composants tiers.

Les composants tiers restent sous leurs licences respectives (zlib, LGPL,
domaine public) ; voir `THIRD_PARTY.md` et `LICENSES/`.

## Reverse engineering et distribution

Le projet repose sur l'analyse et la traduction d'un exécutable que vous
possédez. Les règles varient selon les pays ; les points suivants sont des
repères généraux, pas une autorisation :

- **France / Union européenne** : l'exception de décompilation pour
  interopérabilité (CPI art. L122-6-1 ; directive 2009/24/CE, art. 6) permet
  certains actes de reproduction et de traduction nécessaires à
  l'interopérabilité, sous conditions. Elle ne couvre **pas** la distribution
  du code décompilé ni d'une œuvre dérivée.
- **États-Unis** : le fair use a permis le reverse engineering comme étape
  intermédiaire (*Sega v. Accolade*, *Sony v. Connectix*), mais pas la
  distribution du code ou d'un binaire dérivé.
- **CLUF** : le contrat de licence du jeu peut interdire le reverse
  engineering, la modification ou la redistribution. Un CLUF est un contrat ;
  il peut ajouter des restrictions même là où la loi permet le reverse
  engineering.
- **Mesures techniques de protection** : si votre copie est une version
  no-CD, crackée ou modifiée, cela ne change pas les droits de distribution et
  peut ajouter un problème distinct de contournement de mesure technique
  (DMCA §1201, directive 2009/24/CE). Utilisez une copie légale et non
  modifiée.
- **Droit d'auteur** : *Crazy Taxi 3* (2002) est encore protégé. Le statut
  « abandonware » n'existe pas juridiquement ; l'âge du jeu ne le place pas
  dans le domaine public.

La seule façon de publier légalement le code du jeu ou un binaire qui le
contient est d'obtenir une **autorisation écrite des ayants droit** (SEGA, et
le cas échéant les ayants droit des musiques et des marques présentes). Une
tolérance de fait n'est pas une licence.

## Ce qui peut être publié

Le **code original du projet** peut être publié sous GPL-3.0-or-later :
runtime, recompilateur, outils, documentation, scripts de build. Les
utilisateurs génèrent eux-mêmes le code traduit et le pack depuis leur propre
copie.

Un binaire du runtime **sans code traduit** pourrait être envisagé après
découplage et revue juridique, mais le modèle recommandé reste la publication
du source et des outils. Voir `PUBLISHING.md`.

## Marques

*Crazy Taxi*, *SEGA* et les marques associées appartiennent à leurs
propriétaires. Le projet n'est ni affilié, ni approuvé, ni sponsorisé par SEGA.
L'usage du nom du jeu est nominatif (décrire la compatibilité) ; n'utilisez pas
les logos, jaquettes ou assets du jeu dans la documentation, les captures ou
les pages de release. Conservez la mention d'indépendance du `README.md`.

## Contributions

Les contributions sont acceptées sous GPL-3.0-or-later, avec un
`Signed-off-by` (DCO). Ne joignez jamais de fichier du jeu, de code généré, de
pack, d'asset, de clé ou de lien vers une copie non autorisée. Voir
`CONTRIBUTING.md`.

## Retrait de contenu

Si un ayant droit estime qu'un contenu du dépôt porte atteinte à ses droits,
contactez le mainteneur via le dépôt. Le contenu concerné sera examiné et, le
cas échéant, retiré. Les artefacts dérivés du jeu ne sont pas hébergés ici.

## Absence de garantie

Le projet est fourni « en l'état », sans garantie, dans les limites de la
GPL-3.0-or-later. Vous êtes responsable de l'usage que vous faites du code et
des artefacts que vous générez.
