#enable_language(ASM)

project(ck_memcpy)

add_library(memcpy_jart OBJECT memcpy_jart.c)
set_target_properties(memcpy_jart PROPERTIES COMPILE_FLAGS "-save-temps")

add_library(ck_memcpy STATIC memcpy-impl.h memcpy-impl.cpp memcpy_kernel.c $<TARGET_OBJECTS:memcpy_jart>)
set_target_properties(ck_memcpy PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -fverbose-asm -save-temps")
#target_link_libraries(ck_memcpy PRIVATE fast_memcpy)
