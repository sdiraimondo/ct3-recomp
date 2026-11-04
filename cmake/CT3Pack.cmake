# Pack de données CT3 (lot 1) : extrait .rdata/.data de CT3.exe pour que le
# runtime n'ait plus besoin de l'exécutable Windows. Le pack est un dérivé du
# binaire du jeu : il ne doit pas être versionné ni distribué.
#
# Cibles :
#   ct3pack        génère build/pack/{ct3_data.bin,ct3_image.h,ct3_image.json}
#   ct3pack-check  vérifie les adresses statiques du C généré (build/g3/gen)
#
# Variables :
#   CT3_EXE                 copie de CT3.exe (lecture seule)
#   CT3_PACK_DIR            dossier de sortie (défaut : build/pack)
#   CT3_PACK_INCLUDE_TEXT   inclure .text dans le pack (débogage, défaut : OFF)
#   CT3_PACK_INCLUDE_RSRC   inclure .rsrc (défaut : ON, le runtime ne l'utilise pas encore)

set(CT3_PACK_DIR "${CMAKE_BINARY_DIR}/pack" CACHE PATH "Dossier de sortie du pack de données CT3")
set(CT3_PACK_INCLUDE_TEXT OFF CACHE BOOL "Inclure .text dans le pack (débogage)")
set(CT3_PACK_INCLUDE_RSRC ON CACHE BOOL "Inclure .rsrc dans le pack")
set(CT3_G3_GEN_DIR "${CMAKE_BINARY_DIR}/g3/gen" CACHE PATH "C généré par ct3gen (pour ct3pack-check)")

set(CT3_PEPACK "${PROJECT_SOURCE_DIR}/tools/pepack/pepack.py")
set(CT3_PACK_DATA "${CT3_PACK_DIR}/ct3_data.bin")
set(CT3_PACK_HEADER "${CT3_PACK_DIR}/ct3_image.h")
set(CT3_PACK_MANIFEST "${CT3_PACK_DIR}/ct3_image.json")

set(_ct3_pack_args)
if(CT3_PACK_INCLUDE_TEXT)
  list(APPEND _ct3_pack_args --include-text)
endif()
if(NOT CT3_PACK_INCLUDE_RSRC)
  list(APPEND _ct3_pack_args --no-rsrc)
endif()

if(NOT CT3_EXE)
  add_custom_target(ct3pack
    COMMAND ${CMAKE_COMMAND} -E echo
            "ct3pack : CT3_EXE non défini. Configurer avec -DCT3_EXE=\"/chemin/CT3.exe\"."
    COMMAND ${CMAKE_COMMAND} -E false
    COMMENT "ct3pack : CT3_EXE manquant"
    VERBATIM)
  add_custom_target(ct3pack-check DEPENDS ct3pack)
  return()
endif()

add_custom_command(
  OUTPUT "${CT3_PACK_DATA}" "${CT3_PACK_HEADER}" "${CT3_PACK_MANIFEST}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CT3_PACK_DIR}"
  COMMAND python3 "${CT3_PEPACK}" "${CT3_EXE}" "${CT3_PACK_DIR}" ${_ct3_pack_args}
  DEPENDS "${CT3_PEPACK}" "${CT3_EXE}"
  COMMENT "Génération du pack de données CT3 (ct3pack)"
  VERBATIM)

add_custom_target(ct3pack
  DEPENDS "${CT3_PACK_DATA}" "${CT3_PACK_HEADER}" "${CT3_PACK_MANIFEST}")

# Vérifie que les adresses statiques du C généré tombent dans les sections
# packées. Nécessite ct3gen (build/g3/gen) ; la cible échoue clairement sinon.
add_custom_target(ct3pack-check
  COMMAND python3 "${CT3_PEPACK}" "${CT3_EXE}" "${CT3_PACK_DIR}"
          --scan-generated "${CT3_G3_GEN_DIR}" --strict --self-test ${_ct3_pack_args}
  DEPENDS ct3pack
  COMMENT "Vérification du pack contre les adresses du C généré"
  VERBATIM)
if(TARGET ct3gen)
  add_dependencies(ct3pack-check ct3gen)
endif()
