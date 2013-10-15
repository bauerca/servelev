cmake_minimum_required(VERSION 2.6)
project(servelev)

add_definitions(-DJSMN_PARENT_LINKS -DJSMN_STRICT)

# Build libwebsockets
add_subdirectory(libwebsockets)
add_subdirectory(jsnn)
add_subdirectory(gridfloat)

add_executable(servelev src/main.c)
target_link_libraries(servelev websockets jsmn gf)
