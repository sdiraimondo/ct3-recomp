#!/bin/sh
# Vérifie que le dépôt ne contient aucun artefact dérivé du jeu et que les
# documents de publication sont en place.
#
# Usage : tools/check_no_game_data.sh
# Code de sortie : 0 si tout est propre, 1 sinon.
set -eu

root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "check_no_game_data : dépôt Git introuvable" >&2
    exit 1
}
cd "$root"

# Chemins et extensions qui ne doivent jamais être suivis. Les motifs sont
# volontairement larges : un faux positif se règle en ajoutant une exception
# explicite, pas en affaiblissant le contrôle.
patterns='(^|/)(CT3|ct3)\.(exe|original\.exe)$
(^|/)ct3_data\.bin$
(^|/)ct3_image\.(h|json)$
(^|/)libct3gen\.a$
(^|/)gen_funcs[^/]*\.c$
(^|/)gen_dispatch\.c$
(^|/)pcode\.json$
(^|/)(build|dist|pack|captures|screenshots?|videos?)/'
media='\.(exe|dll|sys|ocx|avi|bik|wav|adx|dds|txd|pak|zip|iso|bin|png|jpe?g|gif|webp|bmp|mp4|webm|log)$'
assets='(^|/)(avi|media)/'

status=0

check_list() {
    label=$1
    list=$2
    if [ -n "$list" ]; then
        echo "ERREUR : $label" >&2
        printf '%s\n' "$list" | sed 's/^/  /' >&2
        status=1
    fi
}

tracked=$(git ls-files)
bad=$(printf '%s\n' "$tracked" | grep -Ei "$patterns" || true)
check_list "fichiers dérivés du jeu suivis" "$bad"

bad=$(printf '%s\n' "$tracked" | grep -Ei "$media" || true)
check_list "fichiers de jeu ou médias suivis" "$bad"

bad=$(printf '%s\n' "$tracked" | grep -Ei "$assets" || true)
check_list "dossiers d'assets suivis" "$bad"

# L'historique compte autant que l'arbre courant : un fichier supprimé reste
# dans les objets Git et serait publié avec le dépôt.
if git rev-parse --git-dir >/dev/null 2>&1; then
    history=$(git log --all --name-only --pretty=format: 2>/dev/null |
              sort -u | grep -Ei "$patterns|$media|$assets" || true)
    check_list "chemins sensibles présents dans l'historique" "$history"
    if git rev-parse --is-shallow-repository 2>/dev/null | grep -q true; then
        echo "AVERTISSEMENT : clone superficiel, l'historique est partiel." >&2
    fi
fi

# Les gros fichiers sont souvent des artefacts de jeu ; on avertit sans
# échouer pour ne pas bloquer un fichier de documentation volumineux.
large=$(git ls-files -z | xargs -0 -r du -k 2>/dev/null |
        awk '$1 >= 2048 {print $2 " (" $1 " Kio)"}' || true)
if [ -n "$large" ]; then
    echo "AVERTISSEMENT : fichiers suivis de plus de 2 Mio :" >&2
    printf '%s\n' "$large" | sed 's/^/  /' >&2
fi

for f in LICENSE THIRD_PARTY.md LEGAL.md CONTRIBUTING.md PUBLISHING.md \
         LICENSES/SDL2.txt LICENSES/DXVK.txt \
         LICENSES/FFmpeg-LGPL-2.1.txt LICENSES/stb_truetype.txt; do
    if [ ! -f "$f" ]; then
        echo "ERREUR : document de publication manquant : $f" >&2
        status=1
    fi
done

if [ -f LICENSE ] && ! grep -q "GNU GENERAL PUBLIC LICENSE" LICENSE; then
    echo "ERREUR : LICENSE ne contient pas le texte de la GPL." >&2
    status=1
fi
if ! grep -q "GPL-3.0-or-later" README.md; then
    echo "ERREUR : README.md ne mentionne pas GPL-3.0-or-later." >&2
    status=1
fi

for entry in 'dist/' 'ct3_data.bin' 'ct3_image.h' 'ct3_image.json' '/pack/'; do
    if ! grep -qxF "$entry" .gitignore; then
        echo "ERREUR : .gitignore ne contient pas : $entry" >&2
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "OK : aucun artefact dérivé du jeu suivi ; documents de publication présents."
fi
exit "$status"
