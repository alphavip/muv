project(myuv)


set(myuvfiles 
          Loop.h
          Loop.cpp
          NetConn.h
          NetConn.cpp
          NetHandler.h
          PktItem.h
          PktItem.cpp
          MemPool.h)


add_library(myuv_a STATIC ${myuvfiles})
target_link_libraries(myuv_a uv_a)

add_executable(myuv Main.cpp)
target_link_libraries(myuv myuv_a)
