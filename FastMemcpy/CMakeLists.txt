project(fast_memcpy)

add_library(fast_memcpy STATIC FastMemcpy.h FastMemcpy.cpp FastMemcpy_Avx.cpp)
set_target_properties(fast_memcpy PROPERTIES COMPILE_FLAGS "-fverbose-asm -save-temps")
