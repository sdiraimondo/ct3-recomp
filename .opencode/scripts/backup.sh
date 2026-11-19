#!/usr/bin/env bash
# Sauvegarde horodatée avant modification d'un fichier existant.
#
# Usage :
#   .opencode/scripts/backup.sh <chemin> [<chemin>...]
#
# Chaque appel crée un répertoire
#   /tmp/opencode/ct3-backups/<horodatage UTC>-<pid>/
# contenant les copies et un manifeste TSV :
#   source, copie, sha256, octets, mode
#
# La variable CT3_BACKUP_ROOT permet de changer la racine (tests).
#
# Garanties :
#   - ne modifie jamais la source ;
#   - refuse un chemin absent ou un nom ambigu (tabulation, saut de ligne) ;
#   - vérifie l'empreinte de chaque copie après écriture ;
#   - n'utilise ni rm, ni mv, ni truncate, ni sed -i.
#
# Restauration : voir docs/ORCHESTRATION.md. Toujours vérifier le manifeste
# avant de recopier une sauvegarde sur la source.

set -euo pipefail

usage() {
    printf 'usage: %s <fichier-ou-dossier>...\n' "${0##*/}" >&2
    exit 2
}

[ "$#" -ge 1 ] || usage

root=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
backup_root=${CT3_BACKUP_ROOT:-/tmp/opencode/ct3-backups}
stamp=$(date -u +%Y%m%dT%H%M%S%NZ)-$$
dest_root="$backup_root/$stamp"

mkdir -p -- "$dest_root"
manifest="$dest_root/manifest.tsv"
printf 'source\tbackup\tsha256\tbytes\tmode\n' > "$manifest"

for src in "$@"; do
    case "$src" in
        *$'\t'* | *$'\n'*)
            printf 'ERREUR: nom de fichier ambigu: %q\n' "$src" >&2
            exit 2
            ;;
    esac
done

sha256_of() {
    sha256sum -- "$1" | awk '{print $1}'
}

record_file() {
    local src=$1 dst=$2 hash bytes mode
    hash=$(sha256_of "$src")
    bytes=$(stat -c '%s' -- "$src")
    mode=$(stat -c '%a' -- "$src")
    printf '%s\t%s\t%s\t%s\t%s\n' "$src" "$dst" "$hash" "$bytes" "$mode" >> "$manifest"
}

verify_file() {
    local src=$1 dst=$2
    if [ ! -f "$dst" ]; then
        printf 'ERREUR: copie absente: %s\n' "$dst" >&2
        return 1
    fi
    if [ "$(sha256_of "$src")" != "$(sha256_of "$dst")" ]; then
        printf 'ERREUR: empreinte différente: %s\n' "$dst" >&2
        return 1
    fi
}

record_link() {
    local src=$1 dst=$2 target
    target=$(readlink -- "$src")
    printf '%s\t%s\tlink:%s\t0\tlink\n' "$src" "$dst" "$target" >> "$manifest"
}

count=0

for src in "$@"; do
    if [ ! -e "$src" ] && [ ! -L "$src" ]; then
        printf 'ERREUR: chemin introuvable: %s\n' "$src" >&2
        exit 3
    fi

    # realpath sert uniquement à placer la copie dans l'arborescence de
    # sauvegarde ; la copie elle-même part de $src pour préserver les liens.
    # On résout le dossier parent mais pas le dernier composant : sinon un
    # lien symbolique serait copié sous le nom de sa cible et écraserait la
    # copie de cette cible.
    abs_dir=$(realpath -m -- "$(dirname -- "$src")")
    abs="$abs_dir/$(basename -- "$src")"
    case "$abs" in
        "$root"/*) rel=${abs#"$root"/} ;;
        *) rel="external/${abs#/}" ;;
    esac
    dst="$dest_root/$rel"

    mkdir -p -- "$(dirname -- "$dst")"

    if [ -L "$src" ]; then
        cp -a -- "$src" "$dst"
        record_link "$abs" "$dst"
        count=$((count + 1))
    elif [ -d "$src" ]; then
        mkdir -p -- "$dst"
        cp -a -- "$src/." "$dst/"
        while IFS= read -r -d '' file; do
            sub=${file#"$abs"/}
            record_file "$file" "$dst/$sub"
            verify_file "$file" "$dst/$sub"
            count=$((count + 1))
        done < <(find "$abs" -type f -print0)
    else
        cp -a -- "$src" "$dst"
        record_file "$abs" "$dst"
        verify_file "$abs" "$dst"
        count=$((count + 1))
    fi
done

cat > "$dest_root/README.txt" <<EOF
Sauvegarde ct3-linux
Date UTC   : $stamp
Dépôt      : $root
Sources    : $#
Fichiers   : $count
Manifeste  : manifest.tsv

Restauration manuelle, après vérification du manifeste :
  cp -a -- <chemin-backup> <chemin-source>

Ne jamais supprimer la source avant d'avoir vérifié l'empreinte.
Les liens symboliques sont copiés tels quels (champ mode = link).
EOF

printf 'Sauvegarde créée : %s\n' "$dest_root"
printf 'Manifeste        : %s\n' "$manifest"
printf 'Fichiers copiés  : %s\n' "$count"
