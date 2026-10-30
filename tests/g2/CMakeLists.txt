# G2 : export p-code de quelques routines de CT3.exe, traduction en C, exécution.

set(G2_JSON "${CMAKE_BINARY_DIR}/g2/pcode.json")
set(G2_GEN "${CMAKE_BINARY_DIR}/g2/gen")

add_custom_command(
  OUTPUT "${G2_JSON}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/g2/ghidra"
  COMMAND ${CMAKE_COMMAND} -E env "JAVA_HOME=${CT3_JAVA_HOME}"
          "${CT3_GHIDRA_DIR}/support/analyzeHeadless"
          "${CMAKE_BINARY_DIR}/g2/ghidra" ct3
          -import "${CT3_EXE}"
          -scriptPath "${PROJECT_SOURCE_DIR}/tools/ghidra"
          -postScript ExportFunctions.java pcode
          "${CMAKE_CURRENT_SOURCE_DIR}/functions.txt" "${G2_JSON}"
          -deleteProject
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/functions.txt" "${PROJECT_SOURCE_DIR}/tools/ghidra/ExportFunctions.java"
  COMMENT "Export p-code Ghidra (G2)"
  VERBATIM)

add_custom_command(
  OUTPUT "${G2_GEN}/gen_funcs.c" "${G2_GEN}/gen_funcs.h" "${G2_GEN}/gen_dispatch.c"
  COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/recomp/ct3rc.py" "${G2_JSON}" "${G2_GEN}"
  DEPENDS "${G2_JSON}" "${PROJECT_SOURCE_DIR}/tools/recomp/ct3rc.py"
  COMMENT "Traduction p-code vers C (G2)"
  VERBATIM)

add_library(ct3_rt STATIC
  "${PROJECT_SOURCE_DIR}/runtime/src/rt_mem.c"
  "${PROJECT_SOURCE_DIR}/runtime/src/rt_pe.c"
  "${PROJECT_SOURCE_DIR}/runtime/src/rt_shims.c")
target_include_directories(ct3_rt PUBLIC
  "${PROJECT_SOURCE_DIR}/runtime/include"
  "${PROJECT_SOURCE_DIR}/runtime/src")

add_library(ct3_gen_g2 STATIC "${G2_GEN}/gen_funcs.c" "${G2_GEN}/gen_dispatch.c")
target_include_directories(ct3_gen_g2 PUBLIC "${G2_GEN}" "${PROJECT_SOURCE_DIR}/runtime/include")
target_compile_options(ct3_gen_g2 PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label)

add_executable(test_g2 test_g2.c)
target_link_libraries(test_g2 PRIVATE ct3_gen_g2 ct3_rt)
