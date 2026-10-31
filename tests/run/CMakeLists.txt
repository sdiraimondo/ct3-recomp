# Exécution de CT3.exe depuis son point d'entrée : code traduit complet (archive produite par run_g3.sh).
set(G3_DIR "${CMAKE_BINARY_DIR}/g3")
set(G3_LIB "${G3_DIR}/libct3gen.a")

add_custom_command(
  OUTPUT "${G3_LIB}"
  COMMAND ${CMAKE_COMMAND} -E env "OUT=${G3_DIR}" REUSE_EXPORT=1 "CT3_EXE=${CT3_EXE}"
          "JAVA_HOME=${CT3_JAVA_HOME}" "GHIDRA_DIR=${CT3_GHIDRA_DIR}"
          bash "${PROJECT_SOURCE_DIR}/tools/recomp/run_g3.sh"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/recomp/run_g3.sh" "${PROJECT_SOURCE_DIR}/tools/recomp/ct3rc.py"
          "${PROJECT_SOURCE_DIR}/tools/ghidra/ExportFunctions.java"
          "${PROJECT_SOURCE_DIR}/runtime/include/rt.h" "${PROJECT_SOURCE_DIR}/runtime/include/rt_fp.h"
          "${PROJECT_SOURCE_DIR}/runtime/include/rt_vec.h"
  COMMENT "Traduction complète de CT3.exe (G3, archive libct3gen.a)"
  VERBATIM)
add_custom_target(ct3gen DEPENDS "${G3_LIB}")

add_executable(ct3_run test_run.c)
target_include_directories(ct3_run PRIVATE "${PROJECT_SOURCE_DIR}/tests/common")
add_dependencies(ct3_run ct3gen)
target_link_libraries(ct3_run PRIVATE "${G3_LIB}" ct3_rt m)
