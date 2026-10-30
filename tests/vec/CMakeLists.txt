# Vecteurs : Unicorn produit les entrées/sorties de référence (tests/vec/vectors.h dans build/).
set(CT3_UNICORN_PYTHON "$ENV{HOME}/.venvs/unicorn-ref/bin/python" CACHE FILEPATH "Python avec Unicorn (référence)")
set(VEC_H "${CMAKE_BINARY_DIR}/vec/vectors.h")

add_custom_command(
  OUTPUT "${VEC_H}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/vec"
  COMMAND "${CT3_UNICORN_PYTHON}" "${PROJECT_SOURCE_DIR}/tools/ref/gen_vectors.py" "${VEC_H}" --count 200
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/ref/gen_vectors.py"
  COMMENT "Vecteurs de référence Unicorn"
  VERBATIM)

add_executable(test_vec test_vec.c "${VEC_H}")
target_include_directories(test_vec PRIVATE "${CMAKE_BINARY_DIR}/vec" "${PROJECT_SOURCE_DIR}/runtime/include")
target_link_libraries(test_vec PRIVATE ct3_rt)
