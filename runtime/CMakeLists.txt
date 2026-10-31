add_library(ct3_rt STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_mem.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_pe.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_shims.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_cpu.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_vec.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_proc.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_thread.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rt_file.c")
target_include_directories(ct3_rt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ct3_rt PUBLIC m pthread)
