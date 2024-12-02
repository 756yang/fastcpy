##############################################################################
##
##  请以`x64本机工具命令行`切换工作目录到本文件路径, 然後执行`nmake`编译
##  请自行配置 gcc 环境变量, 仅适配 gcc/g++ 编译器, 注意!!!
##
##  Copyright (c) Yanglan Network.  All rights reserved.


all: libtfastcpy dllfastcpy_AVX512 dllfastcpy_AVX2 dllfastcpy_SSE2 libfastcpy_AVX512 libfastcpy_AVX2 libfastcpy_SSE2
	copy ..\dylibreloc\tfastcpy.h .
	tar -cvzf ..\libfastcpy.tar.gz fastcpy.h tfastcpy.h lib dylib
	del tfastcpy.h
	move /Y lib\AVX2\libfastcpy.lib ..\lib
	move /Y lib\libtfastcpy.lib ..\lib
	move /Y dylib\libfastcpy.* ..\dylib
	move /Y dylib\libtfastcpy.lib ..\dylib
	del *.obj
	rmdir /S /Q lib
	rmdir /S /Q dylib

libtfastcpy: libfastcpy.dll libfastcpy.lib
	move /Y lib\libfastcpy.lib ..\dylibreloc\lib
	move /Y dylib\libfastcpy.lib ..\dylibreloc\dylib
	copy /Y dylib\libfastcpy.dll ..\dylibreloc\dylib
	pushd ..\dylibreloc & nmake & popd
	copy /Y ..\dylibreloc\lib\libtfastcpy.lib lib
	copy /Y ..\dylibreloc\dylib\libtfastcpy.lib dylib

# 生成动态库, 无任何依赖项的静态初始化 DLL 无需入口函数
libfastcpy.dll: tfastcpy_AVX2.obj tfastcpy_SSE2.obj tfastcpy_AVX512.obj
	link /DLL /DEF:libtfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:dylib\$@

dllfastcpy_AVX512: fastcpy_AVX512.obj
	link /DLL /DEF:libfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:dylib\AVX512\libfastcpy.dll

dllfastcpy_AVX2: fastcpy_AVX2.obj
	link /DLL /DEF:libfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:dylib\AVX2\libfastcpy.dll

dllfastcpy_SSE2: fastcpy_SSE2.obj
	link /DLL /DEF:libfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:dylib\SSE2\libfastcpy.dll

# 生成静态库
libfastcpy.lib: tfastcpy_AVX2.obj tfastcpy_SSE2.obj tfastcpy_AVX512.obj
	lib $? /OUT:lib\$@

libfastcpy_AVX512: fastcpy_AVX512.obj
	lib $? /OUT:lib\AVX512\libfastcpy.lib

libfastcpy_AVX2: fastcpy_AVX2.obj
	lib $? /OUT:lib\AVX2\libfastcpy.lib

libfastcpy_SSE2: fastcpy_SSE2.obj
	lib $? /OUT:lib\SSE2\libfastcpy.lib

build_dir:
	mkdir lib
	mkdir lib\AVX512
	mkdir lib\AVX2
	mkdir lib\SSE2
	mkdir dylib
	mkdir dylib\AVX512
	mkdir dylib\AVX2
	mkdir dylib\SSE2

# 源文件编译
fastcpy_AVX512.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -O3 -DNDEBUG -march=x86-64-v4 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o fastcpy_AVX512.obj

fastcpy_AVX2.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -O3 -DNDEBUG -march=x86-64-v3 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o fastcpy_AVX2.obj

fastcpy_SSE2.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -O3 -DNDEBUG -march=x86-64 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o fastcpy_SSE2.obj


tfastcpy_AVX512.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -DDLLFASTCPY_LIBRARY=0 -DFASTCPY_NO_NTA_PART -O3 -DNDEBUG -march=x86-64-v4 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o tfastcpy_AVX512.obj

tfastcpy_AVX2.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -DDLLFASTCPY_LIBRARY=0 -O3 -DNDEBUG -march=x86-64-v3 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o tfastcpy_AVX2.obj

tfastcpy_SSE2.obj: fastcpy.c build_dir
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -DDLLFASTCPY_LIBRARY=0 -DFASTCPY_NO_NTA_PART -O3 -DNDEBUG -march=x86-64 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o tfastcpy_SSE2.obj

# 本文件使用 UTF8 编码保存会让 nmake 报错 fatal error U1073: 不知道如何生成...

