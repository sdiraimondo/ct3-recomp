# ct3_translated_test(NAME <nom> ADDRS <fichier> SOURCE <fichier.c>)
#
# Export p-code Ghidra des adresses listées, traduction en C (tools/recomp/ct3rc.py),
# compilation de la bibliothèque générée et d'un test qui l'exécute.
# Le test est lié à ct3_rt (chargeur, shims) et reçoit le chemin de CT3.exe en argument.
# Limite : une seule tranche de 300 fonctions au plus (gen_funcs_000.c).

function(ct3_translated_test)
  cmake_parse_arguments(T "" "NAME;ADDRS;SOURCE" "" ${ARGN})
  set(JSON "${CMAKE_BINARY_DIR}/${T_NAME}/pcode.json")
  set(GEN "${CMAKE_BINARY_DIR}/${T_NAME}/gen")

  add_custom_command(
    OUTPUT "${JSON}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${T_NAME}/ghidra"
    COMMAND ${CMAKE_COMMAND} -E env "JAVA_HOME=${CT3_JAVA_HOME}"
            "${CT3_GHIDRA_DIR}/support/analyzeHeadless"
            "${CMAKE_BINARY_DIR}/${T_NAME}/ghidra" ct3
            -import "${CT3_EXE}"
            -scriptPath "${PROJECT_SOURCE_DIR}/tools/ghidra"
            -postScript ExportFunctions.java pcode
            "${T_ADDRS}" "${JSON}"
            -deleteProject
    DEPENDS "${T_ADDRS}" "${PROJECT_SOURCE_DIR}/tools/ghidra/ExportFunctions.java"
    COMMENT "Export p-code Ghidra (${T_NAME})"
    VERBATIM)

  add_custom_command(
    OUTPUT "${GEN}/gen_funcs_000.c" "${GEN}/gen_funcs.h" "${GEN}/gen_dispatch.c"
    COMMAND python3 "${PROJECT_SOURCE_DIR}/tools/recomp/ct3rc.py" "${JSON}" "${GEN}"
    DEPENDS "${JSON}" "${PROJECT_SOURCE_DIR}/tools/recomp/ct3rc.py"
    COMMENT "Traduction p-code vers C (${T_NAME})"
    VERBATIM)

  add_library(${T_NAME}_gen STATIC "${GEN}/gen_funcs_000.c" "${GEN}/gen_dispatch.c")
  target_include_directories(${T_NAME}_gen PUBLIC "${GEN}" "${PROJECT_SOURCE_DIR}/runtime/include")
  target_compile_options(${T_NAME}_gen PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-label)

  add_executable(test_${T_NAME} "${T_SOURCE}")
  target_include_directories(test_${T_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/tests/common")
  target_link_libraries(test_${T_NAME} PRIVATE ${T_NAME}_gen ct3_rt)
endfunction()
