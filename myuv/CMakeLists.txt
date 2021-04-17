project(myuv)

set(files Loop.h
          Loop.cpp
          NetConn.h
          NetConn.cpp
          NetHandler.h
          PktItem.h
          PktItem.cpp
          MemPool.h
          Main.cpp)

include_directories(../include)

add_executable(myuv ${files})

target_link_libraries(myuv uv_a)