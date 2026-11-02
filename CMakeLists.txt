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
set(CMAKE_BUILD_RPATH "${CT3_DEPS_PREFIX}/lib64")

set(CT3_TRACE_INSN "0" CACHE STRING "1 : trace de la dernière instruction invitée (build de débogage)")
set(CT3_EXE "" CACHE FILEPATH "CT3.exe de référence, de votre propre copie (lecture seule)")
if(NOT CT3_EXE)
  message(FATAL_ERROR "CT3_EXE doit désigner votre copie de CT3.exe : -DCT3_EXE=\"/chemin/vers/CT3.exe\"")
endif()
set(CT3_GHIDRA_DIR "$ENV{HOME}/.local/share/ghidra_12.1.4_PUBLIC" CACHE PATH "Installation Ghidra 12.1.4")
set(CT3_JAVA_HOME "" CACHE PATH "JDK 21 pour Ghidra (dossier libexec, ou laisser vide pour le JDK du système)")

include(cmake/CT3Recomp.cmake)

add_subdirectory(runtime)
add_subdirectory(tests/g1_d3d9_triangle)
add_subdirectory(tests/g2)
add_subdirectory(tests/x87)
add_subdirectory(tests/cpu)
add_subdirectory(tests/vec)
add_subdirectory(tests/run)
