project(avx_memmove VERSION 1.0.0 LANGUAGES C)

add_library(avx_memmove STATIC avxmem.h memcmp.c memcpy.c memmove.c memset.c)
set_target_properties(avx_memmove PROPERTIES COMPILE_FLAGS "-ffreestanding -march=x86-64-v3 -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -mno-stack-arg-probe -m64 -mno-red-zone -maccumulate-outgoing-args --std=gnu11 -O3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -fverbose-asm -save-temps")

# -ffreestanding 编译目标运行于独立环境, 无需标准库
# -fno-stack-protector 禁用堆栈保护器
# -fno-stack-check 禁用堆栈溢出检查
# -fno-strict-aliasing 禁用严格的类型别名
# -fno-merge-all-constants 禁止合并相同的常量和相同的变量
# -mno-stack-arg-probe 禁用堆栈探测
# -mno-red-zone 不生成红色区域(System V ABI)
# -maccumulate-outgoing-args 计算传出参数空间并以mov代替push指令











