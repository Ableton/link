cmake_minimum_required(VERSION 3.10)

add_library(abl_link STATIC
  ${CMAKE_CURRENT_LIST_DIR}/src/abl_link.cpp
)

target_include_directories(abl_link PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

set_property(TARGET abl_link PROPERTY C_STANDARD 11)

target_link_libraries(abl_link Ableton::Link)
