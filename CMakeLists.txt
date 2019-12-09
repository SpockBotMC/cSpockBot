cmake_minimum_required(VERSION 3.15)
project(cspock C)

add_executable(main
    src/main.c src/log.c src/sds.c src/sb_event.c src/sb_net.c
)
target_compile_features(main PRIVATE c_std_11)
target_compile_options(main PRIVATE -Wall -Wpedantic)
target_link_libraries(main uv)
target_include_directories(main PRIVATE "include")
