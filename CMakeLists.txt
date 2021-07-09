cmake_minimum_required(VERSION 3.16)
project(Progetto C)

set(CMAKE_C_STANDARD 11)

add_executable(Progetto main.c)

target_link_libraries(Progetto m)