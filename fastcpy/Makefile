##############################################################################
##
##  请以`x64本机工具命令行`切换工作目录到本文件路径, 然後执行`nmake`编译
##  请自行配置 gcc 环境变量, 仅适配 gcc/g++ 编译器, 注意!!!
##
##  Copyright (c) Yanglan Network.  All rights reserved.


all: libfastcpy.lib libfastcpy.dll
	del fastcpy.obj

# 生成动态库, 无任何依赖项的静态初始化 DLL 无需入口函数
libfastcpy.dll: fastcpy.obj
	link /DLL /DEF:libfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:..\dylib\$@

# 生成静态库
libfastcpy.lib: fastcpy.obj
	lib $? /OUT:..\lib\$@

# 源文件编译
fastcpy.obj: fastcpy.c
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -O3 -DNDEBUG -march=x86-64-v3 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o fastcpy.obj

# 本文件使用 UTF8 编码保存会让 nmake 报错 fatal error U1073: 不知道如何生成...

