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
set(CT3_EXE "/run/media/stephane/BC250_USB/Games/Crazy Taxi 3/CT3.exe" CACHE FILEPATH "CT3.exe de référence (lecture seule)")
set(CT3_GHIDRA_DIR "$ENV{HOME}/.local/share/ghidra_12.1.4_PUBLIC" CACHE PATH "Installation Ghidra 12.1.4")
set(CT3_JAVA_HOME "/var/home/linuxbrew/.linuxbrew/Cellar/openjdk@21/21.0.13.0.3/libexec" CACHE PATH "JDK 21 pour Ghidra")

include(cmake/CT3Recomp.cmake)

add_subdirectory(runtime)
add_subdirectory(tests/g1_d3d9_triangle)
add_subdirectory(tests/g2)
add_subdirectory(tests/x87)
add_subdirectory(tests/cpu)
add_subdirectory(tests/vec)
add_subdirectory(tests/run)
