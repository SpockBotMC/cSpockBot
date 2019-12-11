# Previous to 3.15 cmake would override the ASM_NASM_COMPILE_OBJECT
cmake_minimum_required(VERSION 3.15)
# See https://gitlab.kitware.com/cmake/cmake/issues/19453
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
project(cspock C)

add_executable(main
    src/main.c src/log.c src/sds.c src/sb_event.c src/sb_net.c
    src/fibers/fiber.c src/fibers/queue.c src/fibers/scheduler.c
)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall>)
target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wpedantic>)

if(WIN32)
    enable_language(ASM_MASM)
    set(CMAKE_ASM_MASM_FLAGS_DEBUG "/Zi")
    target_sources(main PRIVATE src/fibers/asm/x86_64_win_pe_masm.asm)
else()
    enable_language(ASM_NASM)
    message("In else")
    set(CMAKE_ASM_NASM_LINK_EXECUTABLE "ld <CMAKE_ASM_NASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
    set(CMAKE_ASM_NASM_FLAGS_DEBUG "-g -Fdwarf")
    set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f elf64 -o <OBJECT> <SOURCE>")
    target_sources(main PRIVATE src/fibers/asm/x86_64_sysv_elf_nasm.asm)
endif()

target_link_libraries(main uv)
target_include_directories(main PRIVATE "include")
