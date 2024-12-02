project(fastcpy)

# 为动态库和静态库编译对象文件
add_library(fastcpy_obj OBJECT fastcpy.h fastcpy.c)
set_target_properties(fastcpy_obj PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -save-temps -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper")
#target_compile_definitions(fastcpy_obj PRIVATE FASTCPY_STATIC_LIBRARY)
target_compile_definitions(fastcpy_obj PRIVATE FASTCPY_OWNER_SOURCE)
#target_compile_definitions(fastcpy_obj PRIVATE FASTCPY_UNROLLED_4VEC)
set_property(TARGET fastcpy_obj PROPERTY POSITION_INDEPENDENT_CODE 1)
# 编译 fastcpy 静态库
add_library(fastcpy STATIC $<TARGET_OBJECTS:fastcpy_obj>)
target_compile_definitions(fastcpy PRIVATE FASTCPY_STATIC_LIBRARY)
set_target_properties(fastcpy PROPERTIES OUTPUT_NAME fastcpy)
set_target_properties(fastcpy PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# 编译 fastcpy 动态库
add_library(fastcpy_dynamic SHARED $<TARGET_OBJECTS:fastcpy_obj>)
target_compile_definitions(fastcpy_dynamic PRIVATE FASTCPY_SHARED_LIBRARY)
set_target_properties(fastcpy_dynamic PROPERTIES OUTPUT_NAME fastcpy)
set_target_properties(fastcpy_dynamic PROPERTIES CLEAN_DIRECT_OUTPUT 1)

add_library(memcpy_glibc STATIC test_copy_tiny.c test_copy_medium.c test_copy_large.c x86asm_def.h memcpy_glibc.h memcpy_glibc.c memcpy_glibc_large.c)
# -fcall-used-reg 为函数指定无需保留的寄存器, 用以优化代码生成
set_target_properties(memcpy_glibc PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -save-temps -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper")
# -mno-red-zone 不生成红色区域(System V ABI), 避免内联汇编的 push pop 操作不安全
# -mno-vzeroupper 不自动插入 vzeroupper 指令, 以免破坏 SIMD 寄存器传参
# memcpy_glibc 函数内部以内联汇编保存必要的 xmm 寄存器并插入必要的 vzeroupper 指令
