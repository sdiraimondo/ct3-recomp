# CPUID : profil annoncé par le runtime (sans code traduit).
add_executable(test_cpu test_cpu.c)
target_link_libraries(test_cpu PRIVATE ct3_rt)
