# G2 : chargement de CT3.exe et exécution de trois routines traduites.
ct3_translated_test(
  NAME g2
  ADDRS "${CMAKE_CURRENT_SOURCE_DIR}/functions.txt"
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_g2.c")
