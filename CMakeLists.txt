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

add_subdirectory(tests/g1_d3d9_triangle)
