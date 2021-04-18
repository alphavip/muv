include_directories(libuv/include)
add_subdirectory(libuv)
add_subdirectory (myuv)
SET_PROPERTY(TARGET myuv PROPERTY FOLDER "libuv")