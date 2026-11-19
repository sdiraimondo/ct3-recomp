cmake_minimum_required(VERSION 3.20)
project(ct3-linux C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CT3_DEPS_PREFIX "$ENV{HOME}/.local/ct3-deps/prefix" CACHE PATH "Préfixe des dépendances locales (SDL2, dxvk-native)")
list(APPEND CMAKE_PREFIX_PATH "${CT3_DEPS_PREFIX}")
set(CT3_RPATH "${CT3_DEPS_PREFIX}/lib64" CACHE STRING
    "RPATH des binaires ; utiliser $ORIGIN/../lib pour une release relocalisable")
set(CMAKE_BUILD_RPATH "${CT3_RPATH}")
set(CMAKE_INSTALL_RPATH "${CT3_RPATH}")

set(CT3_TRACE_INSN "0" CACHE STRING "1 : trace de la dernière instruction invitée (build de débogage)")
set(CT3_EXE "" CACHE FILEPATH "CT3.exe de référence, de votre propre copie (lecture seule)")
if(NOT CT3_EXE)
  message(WARNING
    "CT3_EXE non défini : les cibles dépendantes du jeu seront ignorées "
    "(ct3gen, ct3pack, ct3_run, test_g2, test_x87). "
    "Configurer avec -DCT3_EXE=\"/chemin/vers/CT3.exe\" pour les construire.")
endif()
set(CT3_GHIDRA_DIR "$ENV{HOME}/.local/share/ghidra_12.1.4_PUBLIC" CACHE PATH "Installation Ghidra 12.1.4")
set(CT3_JAVA_HOME "" CACHE PATH "JDK 21 pour Ghidra (dossier libexec, ou laisser vide pour le JDK du système)")
set(CT3_PACK_DIR "${CMAKE_BINARY_DIR}/pack" CACHE PATH "Dossier du pack de données CT3 (ct3_data.bin, ct3_image.h)")

include(cmake/CT3Recomp.cmake)

add_subdirectory(runtime)
add_subdirectory(tests/g1_d3d9_triangle)
add_subdirectory(tests/cpu)
add_subdirectory(tests/vec)

# Les cibles suivantes traduisent CT3.exe : sans copie du jeu, elles ne
# peuvent pas être construites. Les ignorer permet de configurer et de
# compiler le runtime et les tests indépendants sur une machine ou une CI
# publique, sans données de jeu.
if(CT3_EXE)
  add_subdirectory(tests/g2)
  add_subdirectory(tests/x87)
  add_subdirectory(tests/run)
else()
  message(STATUS
    "CT3_EXE absent : cibles dépendantes du jeu ignorées "
    "(ct3gen, ct3pack, ct3_run, test_g2, test_x87)")
endif()

include(cmake/CT3Pack.cmake)
