cmake_minimum_required(VERSION 3.22)
project(cw1)

set(CMAKE_CXX_STANDARD 20)

add_compile_options("-fopencilk" "-O3")
add_link_options("-fopencilk")

add_executable(cw1 main.cpp)
