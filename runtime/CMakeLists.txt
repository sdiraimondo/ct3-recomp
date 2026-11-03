find_package(SDL2 REQUIRED)

set(CT3_DXVK_LIB "${CT3_DEPS_PREFIX}/lib64/libdxvk_d3d9.so")
if(NOT EXISTS "${CT3_DXVK_LIB}")
  message(FATAL_ERROR "dxvk-native introuvable : ${CT3_DXVK_LIB}")
endif()

add_library(ct3_rt STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_mem.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_pe.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_shims.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_cpu.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_vec.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_proc.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_filter.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_thread.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_file.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_jmp.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_diag.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_d3d9.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_d3d9_custom.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_d3d9_gen.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_win.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_extra.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_font.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_fs.cpp")
target_include_directories(ct3_rt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CT3_DEPS_PREFIX}/include/dxvk")
target_link_libraries(ct3_rt PUBLIC m pthread SDL2::SDL2 "${CT3_DXVK_LIB}")
