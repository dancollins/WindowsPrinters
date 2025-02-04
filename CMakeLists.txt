cmake_minimum_required(VERSION 3.10)
project(WindowsPrinters)

set(CMAKE_C_STANDARD 11)
add_executable(WindowsPrinters)

target_sources(WindowsPrinters PRIVATE
    src/main.c
)

target_link_libraries(WindowsPrinters PRIVATE
    kernel32
    user32
    gdi32
    winspool
)
