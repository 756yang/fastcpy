cmake_minimum_required (VERSION 3.5)
# 编译器配置必须写在 project 命令之前, 以免 cmake 循环更新 BUG!
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")
project(fastcpy_test)
set(MY_COMPILE_FLAGS "-fexec-charset=UTF-8 -static")
#set(MY_COMPILE_FLAGS "-fexec-charset=UTF-8 -static -mno-vzeroupper")
set(CMAKE_C_FLAGS ${MY_COMPILE_FLAGS})
set(CMAKE_CXX_FLAGS ${MY_COMPILE_FLAGS})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加预处理宏定义
#add_definitions("-DFASTCPY_USE_CMOV")
#-masm=intel
set(CMAKE_BUILD_TYPE "Release")

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
elseif (${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -DNDEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -DNDEBUG")
elseif (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -DNDEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -DNDEBUG")
elseif (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
elseif (${CMAKE_BUILD_TYPE} STREQUAL "BetaTest")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g")
elseif (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebug")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g -DDEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -DDEBUG")
endif()

# 设置链接库目录
link_directories(${PROJECT_SOURCE_DIR}/lib)

# 添加include路径及源文件路径
include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/benchmark/include)
set(INCLUDE_FILE_DIR ./include)
#aux_source_directory(./ SrcFiles)
aux_source_directory(./src SrcFiles)

# 获取头文件列表
set(INCLUDE_FILES ./fastcpy/fastcpy.h)
foreach(LT_INCLUDE_VAR ${INCLUDE_FILE_DIR})
    file(GLOB LT_INCLUDE_VAR RELATIVE ${PROJECT_SOURCE_DIR} ${LT_INCLUDE_VAR}/*.h ${LT_INCLUDE_VAR}/*.hpp)
    set(INCLUDE_FILES ${INCLUDE_FILES} ${LT_INCLUDE_VAR})
endforeach()
#file(WRITE print.txt "${INCLUDE_FILES} ${SrcFiles}")

# 设置输出目录
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# google-test bug: https://github.com/google/benchmark/issues/1845
set(BENCHMARK_ENABLE_GTEST_TESTS FALSE)
# benchmark warning: enumeration value 'CacheUnknown' not handled in switch [-Wswitch]
set(BENCHMARK_ENABLE_WERROR OFF)
set(BENCHMARK_ENABLE_TESTING OFF)
add_subdirectory(benchmark)
add_subdirectory(fastcpy)
# 编译其他 memcpy 实现, 用于测试
add_subdirectory(AVX-Memmove)
add_subdirectory(rte_memcpy)
add_subdirectory(FastMemcpy)
add_subdirectory(ck_memcpy)
add_subdirectory(memcpy-amd64)

# 生成可执行文件并设置输出目录
set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)
add_executable(fastcpy_bench fastcpy_bench.cpp ${INCLUDE_FILES} ${SrcFiles})
target_compile_definitions(fastcpy_bench PRIVATE FASTCPY_OWNER_SOURCE)
#set_target_properties(fastcpy_bench PROPERTIES OUTPUT_NAME fastcpy)
set_target_properties(fastcpy_bench PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -save-temps")
# 为目标链接可执行库
add_dependencies(fastcpy_bench fastcpy)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib .dll ${CMAKE_FIND_LIBRARY_SUFFIXES})
find_library(FASTCPY_LIB fastcpy ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(fastcpy_bench PRIVATE ${FASTCPY_LIB})
#target_link_libraries(fastcpy_bench PRIVATE fastcpy)
target_link_libraries(fastcpy_bench PRIVATE memcpy_glibc)
target_link_libraries(fastcpy_bench PRIVATE avx_memmove)
target_link_libraries(fastcpy_bench PRIVATE rte_memcpy)
target_link_libraries(fastcpy_bench PRIVATE fast_memcpy)
target_link_libraries(fastcpy_bench PRIVATE ck_memcpy)
target_link_libraries(fastcpy_bench PRIVATE memcpy_amd64)
target_link_libraries(fastcpy_bench PRIVATE benchmark)

# 测试工具构建
add_executable(fastcpy_test fastcpy_test.cpp)
target_compile_definitions(fastcpy_test PRIVATE FASTCPY_OWNER_SOURCE)
set_target_properties(fastcpy_test PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -save-temps")
# 为目标链接可执行库
add_dependencies(fastcpy_test fastcpy)
target_link_libraries(fastcpy_test PRIVATE ${FASTCPY_LIB})

# cpuid non_temporal_threshold test tool
add_executable(non_temporal_threshold cpucacheinfo_new.c)
