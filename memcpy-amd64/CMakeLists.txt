project(memcpy_amd64)

set(CMAKE_CXX_STANDARD 17)

add_library(memcpy_amd64 STATIC memcpy.cpp)

set_target_properties(memcpy_amd64 PROPERTIES COMPILE_FLAGS "-march=x86-64-v3 -fverbose-asm -save-temps")
