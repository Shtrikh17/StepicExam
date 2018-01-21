cmake_minimum_required(VERSION 2.0)
project(SimpleHTTPserver C)

set(CMAKE_C_STANDARD 99)
add_executable(SimpleHTTPserver main.c myHTTP.c myHTTP.h commonOperations.c commonOperations.h server.c server.h)
target_link_libraries(SimpleHTTPserver event)
target_link_libraries(SimpleHTTPserver pthread)
