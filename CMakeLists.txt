# Previous to 3.15 cmake would override the ASM_NASM_COMPILE_OBJECT
cmake_minimum_required(VERSION 3.15)
# See https://gitlab.kitware.com/cmake/cmake/issues/19453
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
project(cspock C)

set(MC_PROTO_VER 1_15_1)
set(PROTO_INCLUDE \"${MC_PROTO_VER}_proto.h\")

find_package(Python3)
add_custom_command(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gen_proto.py ${MC_PROTO_VER}
    DEPENDS gen_proto.py
    OUTPUT src/${MC_PROTO_VER}_proto.c include/${MC_PROTO_VER}_proto.h
    COMMENT "Generating protocol files"
)

add_executable(main
    src/main.c             src/log.c              src/sds.c
    src/event/sb_event.c   src/net/sb_net.c       src/${MC_PROTO_VER}_proto.c
    src/datautils.c        src/cNBT/buffer.c      src/cNBT/nbt_loading.c
    src/cNBT/nbt_parsing.c src/cNBT/nbt_treeops.c src/cNBT/nbt_util.c
    src/fibers/fiber.c     src/fibers/queue.c     src/fibers/scheduler.c
    src/base/base.c
)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall>)
target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wpedantic>)
target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wno-pointer-sign>)
target_compile_definitions(main PRIVATE MC_PROTO_INCLUDE=${PROTO_INCLUDE})

if(WIN32)
    enable_language(ASM_MASM)
    set(CMAKE_ASM_MASM_FLAGS_DEBUG "/Zi")
    target_sources(main PRIVATE src/fibers/asm/x86_64_win_pe_masm.asm)
else()
    enable_language(ASM_NASM)
    set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
    set(CMAKE_ASM_NASM_FLAGS_DEBUG "-g -Fdwarf")
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f elf64 -o <OBJECT> <SOURCE>")
    target_sources(main PRIVATE src/fibers/asm/x86_64_sysv_elf_nasm.asm)
endif()

target_link_libraries(main uv z)
target_include_directories(main PRIVATE include ${CMAKE_CURRENT_BINARY_DIR}/include)


add_executable(test
    src/test.c             src/log.c              src/sds.c
    src/datautils.c        src/cNBT/buffer.c      src/cNBT/nbt_loading.c
    src/cNBT/nbt_parsing.c src/cNBT/nbt_treeops.c src/cNBT/nbt_util.c
)
target_link_libraries(test z)
target_include_directories(test PRIVATE include)
