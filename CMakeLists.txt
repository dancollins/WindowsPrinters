cmake_minimum_required(VERSION 3.10)
project(WindowsPrinters)

set(CMAKE_C_STANDARD 11)

add_executable(ListPrinters)

target_sources(ListPrinters PRIVATE
    src/list_details.c
)

target_link_libraries(ListPrinters PRIVATE
    kernel32
    user32
    gdi32
    winspool
)

add_executable(DemoPrint)

target_sources(DemoPrint PRIVATE
    src/demo_print.c
)

target_link_libraries(DemoPrint PRIVATE
    kernel32
    user32
    gdi32
    winspool
)

add_executable(DatamatrixPrint)

target_sources(DatamatrixPrint PRIVATE
    src/datamatrix_print.c
)

target_link_libraries(DatamatrixPrint PRIVATE
    kernel32
    user32
    gdi32
    winspool
)

target_link_libraries(DatamatrixPrint PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libdmtx/libdmtx.a)
target_include_directories(DatamatrixPrint PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libdmtx)
