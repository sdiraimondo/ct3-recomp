#!/usr/bin/env bash
# Cross-build reproductible du squelette WSI Switch de DXVK 3.1.1
# (devkitA64/libnx, backend VK_NN_vi_surface) pour le jalon S4.1.
#
# Objet :
#   Valide le critère d'acceptation S4.1 (« Compilation avec -DDXVK_WSI_SWITCH »,
#   docs/ROADMAP.md) : configuration Meson en mode natif Switch
#   (-Dnative_switch=true), présence effective de -DDXVK_WSI_SWITCH dans les
#   commandes de compilation, et compilation des 3 objets du squelette WSI
#   Switch (switch_wsi_platform_switch.cpp.o, switch_wsi_window_switch.cpp.o,
#   switch_wsi_monitor_switch.cpp.o) dans un conteneur devkitPro, sans modifier
#   l'arbre source.
#   L'archive src/wsi/libwsi.a est une étape informative (tentée après les 3
#   objets) : son échec connu hors S4.1 ne fait pas échouer le script, sauf si
#   CT3_DXVK_REQUIRE_LIBWSI=1.
#
# Prérequis :
#   - podman installé et exécutable ;
#   - image devkitPro contenant meson, ninja, devkitA64 et libnx
#     (par défaut : localhost/devkitpro-mesa-rust:latest) ;
#   - accès réseau du conteneur pour installer glslang-tools (glslangValidator)
#     si l'image n'en fournit pas ;
#   - arbre source DXVK 3.1.1 patché (WSI Switch) accessible en lecture.
#
# Usage :
#   tools/port/dxvk_switch_wsi_build.sh
#   DXVK_SRC=/chemin/vers/dxvk tools/port/dxvk_switch_wsi_build.sh
#
# Variables d'environnement :
#   DXVK_SRC
#       Arbre source DXVK 3.1.1 patché. Défaut :
#       $HOME/.local/ct3-deps/src/dxvk
#       Doit contenir meson.build et src/wsi/switch/, sinon erreur explicite.
#   DEVKIT_IMAGE
#       Image podman devkitPro. Défaut : localhost/devkitpro-mesa-rust:latest
#   CT3_DXVK_BUILD_DIR
#       Répertoire de build hôte (créé, jamais supprimé). Défaut :
#       /tmp/opencode/ct3-dxvk-switch-build-$$
#   CT3_DXVK_REQUIRE_LIBWSI
#       Si défini à 1, l'échec de production de src/wsi/libwsi.a devient
#       bloquant (RESULT=FAIL). Par défaut informatif (utile quand le blocage
#       cœur DXVK hors S4.1 sera levé en S7).
#
# Sortie :
#   RESULT=PASS et code 0 si la configuration (native_switch=true,
#   -DDXVK_WSI_SWITCH accepté) et la compilation des 3 objets Switch réussissent,
#   et si l'archive libwsi.a est produite ou si son échec reste informatif
#   (CT3_DXVK_REQUIRE_LIBWSI != 1) ;
#   RESULT=FAIL et code non nul sinon. Les journaux sont conservés dans le
#   répertoire de build, dont le chemin est affiché.
#
# Exemple de sortie attendue :
#   Vérification configuration : native_switch=true détecté.
#   Vérification configuration : -DDXVK_WSI_SWITCH accepté/détecté.
#   Vérification objets WSI    : 3/3 objets Switch compilés.
#   Archive libwsi.a           : non produite (blocage connu hors S4.1, voir NOTE).
#   RESULT=PASS
#   Journaux conservés dans : /tmp/opencode/ct3-dxvk-switch-build-12345
#
# Références :
#   - docs/SWITCH_PORT.md §3 (détail WSI DXVK, blocages et fichiers ciblés)
#   - docs/ROADMAP.md (critère d'acceptation S4.1)
#   - tools/port/dxvk-3.1.1-switch-wsi-s4.1.patch (correctif meson/WSI S4.1)
#
# Aucune donnée de jeu n'est utilisée ; CT3.exe n'est pas lu ; l'arbre source
# DXVK est monté en lecture seule.

set -euo pipefail

DXVK_SRC="${DXVK_SRC:-$HOME/.local/ct3-deps/src/dxvk}"
DEVKIT_IMAGE="${DEVKIT_IMAGE:-localhost/devkitpro-mesa-rust:latest}"
CT3_DXVK_BUILD_DIR="${CT3_DXVK_BUILD_DIR:-/tmp/opencode/ct3-dxvk-switch-build-$$}"
CT3_DXVK_REQUIRE_LIBWSI="${CT3_DXVK_REQUIRE_LIBWSI:-0}"

print_result_fail() {
    local build_dir_msg="${CT3_DXVK_BUILD_DIR:-<non créé>}"
    echo "RESULT=FAIL"
    echo "Journaux conservés dans : ${build_dir_msg}"
}

# 1) Validation de l'arbre source DXVK
if [ ! -d "$DXVK_SRC" ]; then
    echo "ERREUR: DXVK_SRC n'est pas un répertoire : $DXVK_SRC" >&2
    echo "        Définissez DXVK_SRC vers un arbre DXVK 3.1.1 patché (meson.build + src/wsi/switch/)." >&2
    print_result_fail
    exit 1
fi
if [ ! -f "$DXVK_SRC/meson.build" ] || [ ! -d "$DXVK_SRC/src/wsi/switch" ]; then
    echo "ERREUR: l'arbre DXVK_SRC ne semble pas être un DXVK 3.1.1 patché pour Switch." >&2
    echo "        Attendu : $DXVK_SRC/meson.build et $DXVK_SRC/src/wsi/switch/" >&2
    echo "        DXVK_SRC actuel : $DXVK_SRC" >&2
    print_result_fail
    exit 1
fi

# 2) Vérification de podman et de l'image
if ! command -v podman >/dev/null 2>&1; then
    echo "ERREUR: podman est requis mais introuvable dans PATH." >&2
    echo "        Installez podman (ex. paquet système) puis relancez le script." >&2
    print_result_fail
    exit 1
fi
if ! podman image exists "$DEVKIT_IMAGE"; then
    echo "ERREUR: image podman absente : $DEVKIT_IMAGE" >&2
    echo "        Construisez ou tirez une image devkitPro avec meson, ninja, devkitA64 et libnx," >&2
    echo "        puis relancez (ou définissez DEVKIT_IMAGE vers l'image disponible)." >&2
    echo "        Exemple : podman image exists localhost/devkitpro-mesa-rust:latest" >&2
    print_result_fail
    exit 1
fi

# 3) Préparation du répertoire de build (jamais supprimé)
mkdir -p "$CT3_DXVK_BUILD_DIR"
CONTAINER_LOG="$CT3_DXVK_BUILD_DIR/container.log"

echo "DXVK_SRC          : $DXVK_SRC"
echo "DEVKIT_IMAGE      : $DEVKIT_IMAGE"
echo "CT3_DXVK_BUILD_DIR: $CT3_DXVK_BUILD_DIR"
echo "CT3_DXVK_REQUIRE_LIBWSI: $CT3_DXVK_REQUIRE_LIBWSI"
echo

# 4) Exécution dans le conteneur (source en lecture seule)
#    Étapes obligatoires (S4.1) : setup + 3 objets Switch.
#    Étape informative : archive libwsi.a (échec non bloquant sauf mode strict).
CONTAINER_RC=0
podman run --rm \
    --security-opt label=disable \
    -v "$DXVK_SRC:/src:ro" \
    -v "$CT3_DXVK_BUILD_DIR:/out" \
    "$DEVKIT_IMAGE" bash -lc '
set -euo pipefail
export DEVKITPRO=/opt/devkitpro

echo "[conteneur] DEVKITPRO=$DEVKITPRO"
if ! command -v glslangValidator >/dev/null 2>&1; then
    echo "[conteneur] glslangValidator absent, installation de glslang-tools (réseau requis)..."
    if ! (apt-get update -qq && apt-get install -y -qq glslang-tools); then
        echo "[conteneur] ERREUR: installation de glslang-tools impossible (apt-get)." >&2
        echo "[conteneur]         Vérifiez l’accès réseau du conteneur et les dépôts APT de l’image." >&2
        exit 10
    fi
    echo "[conteneur] glslang-tools installé."
else
    echo "[conteneur] glslangValidator déjà présent."
fi

echo "[conteneur] génération du fichier cross avec /opt/devkitpro/meson-toolchain.sh switch"
if ! /opt/devkitpro/meson-toolchain.sh switch > /out/switch.cross; then
    echo "[conteneur] ERREUR: génération du fichier cross échouée." >&2
    exit 11
fi
echo "[conteneur] fichier cross écrit : /out/switch.cross"

echo "[conteneur] configuration Meson (native_switch=true, --buildtype=release)"
meson setup \
    --cross-file=/out/switch.cross \
    -Dnative_switch=true \
    --buildtype=release \
    /out/build \
    /src

echo "[conteneur] compilation des 3 objets WSI Switch (obligatoire, critère S4.1)"
ninja -C /out/build \
    src/wsi/libwsi.a.p/switch_wsi_platform_switch.cpp.o \
    src/wsi/libwsi.a.p/switch_wsi_window_switch.cpp.o \
    src/wsi/libwsi.a.p/switch_wsi_monitor_switch.cpp.o
echo "[conteneur] 3 objets WSI Switch compilés."

echo "[conteneur] tentative de production de src/wsi/libwsi.a (informative, hors S4.1)"
set +e
ninja -C /out/build src/wsi/libwsi.a
LIBWSI_RC=$?
set -e
echo "$LIBWSI_RC" > /out/libwsi.rc
if [ "$LIBWSI_RC" -eq 0 ]; then
    echo "[conteneur] Archive libwsi.a produite."
else
    echo "NOTE: archive libwsi.a non produite (blocage connu, hors S4.1) :"
    echo "      src/wsi/wsi_edid.cpp -> src/util/thread.h : pthread_setschedparam absent sur Horizon/newlib,"
    echo "      problème de portabilité du cœur DXVK à traiter en S7."
fi

echo "[conteneur] étapes obligatoires S4.1 terminées"
' 2>&1 | tee "$CONTAINER_LOG" || CONTAINER_RC=$?

# 5) Vérifications post-build (configuration et artefacts)
CONFIG_OK=0
if [ -f "$CT3_DXVK_BUILD_DIR/build/meson-info/intro-buildoptions.json" ]; then
    if grep -q '"name"[[:space:]]*:[[:space:]]*"native_switch"' "$CT3_DXVK_BUILD_DIR/build/meson-info/intro-buildoptions.json" \
       && grep -q '"value"[[:space:]]*:[[:space:]]*true' "$CT3_DXVK_BUILD_DIR/build/meson-info/intro-buildoptions.json"; then
        CONFIG_OK=1
    fi
fi
if [ "$CONFIG_OK" -ne 1 ] && [ -f "$CONTAINER_LOG" ]; then
    # Repli : analyse textuelle de la sortie de configuration
    if grep -q 'native_switch' "$CONTAINER_LOG" && grep -Eq 'native_switch[[:space:]]*:[[:space:]]*true|native_switch.*true' "$CONTAINER_LOG"; then
        CONFIG_OK=1
    elif grep -q 'native_switch' "$CONTAINER_LOG" && grep -q 'true' "$CONTAINER_LOG"; then
        CONFIG_OK=1
    fi
fi

WSI_DEFINE_OK=0
if [ -f "$CT3_DXVK_BUILD_DIR/build/build.ninja" ]; then
    if grep -q 'DXVK_WSI_SWITCH' "$CT3_DXVK_BUILD_DIR/build/build.ninja"; then
        WSI_DEFINE_OK=1
    fi
fi
if [ "$WSI_DEFINE_OK" -ne 1 ] && [ -f "$CT3_DXVK_BUILD_DIR/build/meson-info/intro-targets.json" ]; then
    if grep -q 'DXVK_WSI_SWITCH' "$CT3_DXVK_BUILD_DIR/build/meson-info/intro-targets.json"; then
        WSI_DEFINE_OK=1
    fi
fi
if [ "$WSI_DEFINE_OK" -ne 1 ] && [ -f "$CONTAINER_LOG" ]; then
    if grep -q 'DXVK_WSI_SWITCH' "$CONTAINER_LOG"; then
        WSI_DEFINE_OK=1
    fi
fi

SWITCH_OBJECTS=(
    "$CT3_DXVK_BUILD_DIR/build/src/wsi/libwsi.a.p/switch_wsi_platform_switch.cpp.o"
    "$CT3_DXVK_BUILD_DIR/build/src/wsi/libwsi.a.p/switch_wsi_window_switch.cpp.o"
    "$CT3_DXVK_BUILD_DIR/build/src/wsi/libwsi.a.p/switch_wsi_monitor_switch.cpp.o"
)
OBJECTS_COUNT=0
for obj in "${SWITCH_OBJECTS[@]}"; do
    if [ -f "$obj" ]; then
        OBJECTS_COUNT=$((OBJECTS_COUNT + 1))
    fi
done

LIBWSI_PATH="$CT3_DXVK_BUILD_DIR/build/src/wsi/libwsi.a"
LIBWSI_RC_FILE="$CT3_DXVK_BUILD_DIR/libwsi.rc"
LIBWSI_RC=""
if [ -f "$LIBWSI_RC_FILE" ]; then
    LIBWSI_RC="$(tr -d '[:space:]' < "$LIBWSI_RC_FILE" 2>/dev/null || true)"
fi
LIBWSI_PRODUCED=0
LIBWSI_SIZE=""
if [ "$LIBWSI_RC" = "0" ] && [ -f "$LIBWSI_PATH" ]; then
    LIBWSI_PRODUCED=1
    LIBWSI_SIZE="$(wc -c < "$LIBWSI_PATH" 2>/dev/null | tr -d '[:space:]' || true)"
fi

MANDATORY_OK=1
if [ "$CONTAINER_RC" -ne 0 ] || [ "$CONFIG_OK" -ne 1 ] || [ "$WSI_DEFINE_OK" -ne 1 ] || [ "$OBJECTS_COUNT" -ne 3 ]; then
    MANDATORY_OK=0
fi

ARCHIVE_BLOCKING=0
if [ "$CT3_DXVK_REQUIRE_LIBWSI" = "1" ] && [ "$LIBWSI_PRODUCED" -ne 1 ]; then
    ARCHIVE_BLOCKING=1
fi

echo
if [ "$CONFIG_OK" -eq 1 ]; then
    echo "Vérification configuration : native_switch=true détecté."
else
    echo "Vérification configuration : native_switch=true non confirmé dans les journaux/introspection." >&2
fi
if [ "$WSI_DEFINE_OK" -eq 1 ]; then
    echo "Vérification configuration : -DDXVK_WSI_SWITCH accepté/détecté."
else
    echo "Vérification configuration : -DDXVK_WSI_SWITCH non confirmé dans les commandes de compilation." >&2
fi
echo "Vérification objets WSI    : ${OBJECTS_COUNT}/3 objets Switch compilés."

if [ "$LIBWSI_PRODUCED" -eq 1 ]; then
    if [ -n "$LIBWSI_SIZE" ]; then
        echo "Archive libwsi.a           : produite (${LIBWSI_SIZE} octets)."
    else
        echo "Archive libwsi.a           : produite."
    fi
elif [ -n "$LIBWSI_RC" ]; then
    echo "Archive libwsi.a           : non produite (blocage connu hors S4.1, voir NOTE)."
    echo "NOTE: archive libwsi.a non produite (blocage connu, hors S4.1) :"
    echo "      src/wsi/wsi_edid.cpp -> src/util/thread.h : pthread_setschedparam absent sur Horizon/newlib,"
    echo "      problème de portabilité du cœur DXVK à traiter en S7."
    if [ "$CT3_DXVK_REQUIRE_LIBWSI" = "1" ]; then
        echo "NOTE: CT3_DXVK_REQUIRE_LIBWSI=1 : l'échec de l'archive est bloquant." >&2
    fi
else
    echo "Archive libwsi.a           : non tentée (étapes obligatoires incomplètes)."
fi

if [ "$CONTAINER_RC" -ne 0 ]; then
    echo "Code retour conteneur (étapes obligatoires) : $CONTAINER_RC" >&2
fi
if [ -n "$LIBWSI_RC" ]; then
    echo "Code retour archive libwsi.a                : $LIBWSI_RC"
fi

if [ "$MANDATORY_OK" -eq 1 ] && [ "$ARCHIVE_BLOCKING" -eq 0 ]; then
    echo "RESULT=PASS"
    echo "Journaux conservés dans : $CT3_DXVK_BUILD_DIR"
    exit 0
fi

print_result_fail
exit 1
