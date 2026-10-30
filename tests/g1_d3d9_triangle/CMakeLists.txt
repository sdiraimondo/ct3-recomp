find_package(SDL2 REQUIRED)

set(DXVK_NATIVE_LIB "${CT3_DEPS_PREFIX}/lib64/libdxvk_d3d9.so")
set(DXVK_NATIVE_INCLUDE "${CT3_DEPS_PREFIX}/include/dxvk")

if(NOT EXISTS "${DXVK_NATIVE_LIB}")
  message(FATAL_ERROR "dxvk-native introuvable : ${DXVK_NATIVE_LIB}")
endif()

add_executable(g1_d3d9_triangle main.cpp)
target_include_directories(g1_d3d9_triangle PRIVATE "${DXVK_NATIVE_INCLUDE}")
target_link_libraries(g1_d3d9_triangle PRIVATE SDL2::SDL2 "${DXVK_NATIVE_LIB}")
set_target_properties(g1_d3d9_triangle PROPERTIES BUILD_RPATH "${CT3_DEPS_PREFIX}/lib64")
