project(myserver)
cmake_minimum_required(VERSION 3.4)

include_directories(libuv/include)
include_directories(myuv)

add_subdirectory(libuv)
add_subdirectory(myuv)
add_subdirectory(gateway)
