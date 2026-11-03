find_package(SDL2 REQUIRED)

set(CT3_DXVK_LIB "${CT3_DEPS_PREFIX}/lib64/libdxvk_d3d9.so")
if(NOT EXISTS "${CT3_DXVK_LIB}")
  message(FATAL_ERROR "dxvk-native introuvable : ${CT3_DXVK_LIB}")
endif()

# Écran de chargement (avi/loading.avi) : FFmpeg optionnel, cherché dans le préfixe des dépendances.
find_package(PkgConfig QUIET)
set(CT3_HAVE_FFMPEG 0)
if(PkgConfig_FOUND)
  set(_ct3_pc_save "$ENV{PKG_CONFIG_PATH}")
  set(ENV{PKG_CONFIG_PATH} "${CT3_DEPS_PREFIX}/lib/pkgconfig:${CT3_DEPS_PREFIX}/lib64/pkgconfig:$ENV{PKG_CONFIG_PATH}")
  pkg_check_modules(CT3_FFMPEG QUIET IMPORTED_TARGET libavformat libavcodec libswscale libavutil)
  set(ENV{PKG_CONFIG_PATH} "${_ct3_pc_save}")
endif()
if(CT3_FFMPEG_FOUND)
  set(CT3_HAVE_FFMPEG 1)
  message(STATUS "FFmpeg trouvé : écran de chargement vidéo activé")
else()
  message(STATUS "FFmpeg absent : pas d'écran de chargement vidéo")
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
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_fs.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_splash.c")
target_include_directories(ct3_rt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CT3_DEPS_PREFIX}/include/dxvk")
target_link_libraries(ct3_rt PUBLIC m pthread SDL2::SDL2 "${CT3_DXVK_LIB}")
target_compile_definitions(ct3_rt PUBLIC CT3_HAVE_FFMPEG=${CT3_HAVE_FFMPEG})
if(CT3_FFMPEG_FOUND)
  target_link_libraries(ct3_rt PUBLIC PkgConfig::CT3_FFMPEG)
endif()
