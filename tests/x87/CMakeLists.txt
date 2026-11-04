# Tests x87 : routines réelles de CT3.exe traduites, pack de données CT3.
ct3_translated_test(
  NAME x87
  ADDRS "${CMAKE_CURRENT_SOURCE_DIR}/functions.txt"
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_x87.c")
