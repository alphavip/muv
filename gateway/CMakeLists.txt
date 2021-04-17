project(gateway)

set(gatewayfiles 
    Main.cpp)

add_executable(gateway ${gatewayfiles})

target_link_libraries(gateway myuv_a)