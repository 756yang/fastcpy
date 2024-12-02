project(rte_memcpy VERSION 1.0.0 LANGUAGES C)

add_library(rte_memcpy_avx OBJECT rte_memcpy.h rte_memcpy.c)
set_target_properties(rte_memcpy_avx PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -fverbose-asm -save-temps")

add_library(rte_memcpy_sse OBJECT rte_memcpy.h rte_memcpy.c)
set_target_properties(rte_memcpy_sse PROPERTIES COMPILE_FLAGS "-march=x86-64-v2 -fverbose-asm -save-temps")

add_library(rte_memcpy STATIC $<TARGET_OBJECTS:rte_memcpy_avx> $<TARGET_OBJECTS:rte_memcpy_sse>)
