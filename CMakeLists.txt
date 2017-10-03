
cmake_minimum_required(VERSION 2.6)

project(pfs)

# Build options
add_definitions(-O2 -fomit-frame-pointer -std=c89)
# Warnings
add_definitions(-Wall -Wextra -Wredundant-decls -Wno-unused-result -Wno-strict-aliasing)

add_executable(pfs main.c opt.c)

target_link_libraries(pfs -lpfs)

install(TARGETS pfs DESTINATION bin)
