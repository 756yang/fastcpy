##############################################################################
##
##  请以`x64本机工具命令行`切换工作目录到本文件路径, 然後执行`nmake`编译
##  请自行配置 gcc 环境变量, 仅适配 gcc/g++ 编译器, 注意!!!
##
##  Copyright (c) Yanglan Network.  All rights reserved.

# 注意, Windows有导入库机制, 因此程序仅需链接 libdllfastcpy.lib
all: libimpdllfastcpy.lib libimplibfastcpy.lib
	del *.obj

# 将 dylibreloc 与指定动态库的导入库打包
libimpdllfastcpy.lib: dylibreloc_shared.obj
	lib $? dylib\libtfastcpy.lib /OUT:bin\$@

# 将 dylibreloc 与指定静态库打包
libimplibfastcpy.lib: dylibreloc_static.obj
	lib $? lib\libtfastcpy.lib /OUT:bin\$@

# 最小体积编译 dylibreloc
dylibreloc_shared.obj: dylibreloc.c
	gcc -m64 -L./dylib -ltfastcpy -c -Os -DNDEBUG -DDYLIB_RELOC_USE_MUTEX dylibreloc.c -o dylibreloc_shared.obj

dylibreloc_static.obj: dylibreloc.c
	gcc -m64 -L./lib -ltfastcpy -c -Os -DNDEBUG -DFASTCPY_STATIC_LIBRARY -DDYLIB_RELOC_USE_MUTEX dylibreloc.c -o dylibreloc_static.obj
