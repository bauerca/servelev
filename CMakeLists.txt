cmake_minimum_required(VERSION 2.6)
project(servelev)

add_definitions(-DJSMN_PARENT_LINKS -DJSMN_STRICT)

# Build libwebsockets
add_subdirectory(libwebsockets)
add_subdirectory(jsnn)
add_subdirectory(gridfloat)

set(SOURCES
  src/main.c
  src/parse_request.c
)

add_executable(servelev ${SOURCES})
set_target_properties(servelev PROPERTIES COMPILE_FLAGS "-g")
target_link_libraries(servelev websockets jsnn gf)
