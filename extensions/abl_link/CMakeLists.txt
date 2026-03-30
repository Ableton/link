#  _ _       _        _           _
# | (_)_ __ | | __   | |__  _   _| |_
# | | | '_ \| |/ /   | '_ \| | | | __|
# | | | | | |   <    | | | | |_| | |_
# |_|_|_| |_|_|\_\___|_| |_|\__,_|\__|
#               |_____|

set(link_hut_SOURCES
   ${CMAKE_CURRENT_LIST_DIR}/examples/link_hut/main.c
)

source_group("link_hut" FILES ${link_hut_SOURCES})

add_executable(link_hut ${link_hut_SOURCES})

set_property(TARGET link_hut PROPERTY C_STANDARD 11)

target_link_libraries(link_hut abl_link)


#
# | (_)_ __ | | __    __ _ _   _  __| (_) ___    | |__  _   _| |_
# | | | '_ \| |/ /   / _` | | | |/ _` | |/ _ \   | '_ \| | | | __|
# | | | | | |   <   | (_| | |_| | (_| | | (_) |  | | | | |_| | |_
# |_|_|_| |_|_|\_\___\__,_|\__,_|\__,_|_|\___/___|_| |_|\__,_|\__|
#               |_____|                     |_____|

set(link_audio_hut_SOURCES
   ${CMAKE_CURRENT_LIST_DIR}/examples/link_audio_hut/main.c
)

source_group("link_audio_hut" FILES ${link_audio_hut_SOURCES})

add_executable(link_audio_hut ${link_audio_hut_SOURCES})

set_property(TARGET link_audio_hut PROPERTY C_STANDARD 11)
set_property(TARGET link_audio_hut PROPERTY C_STANDARD_REQUIRED ON)
if(MSVC)
  target_compile_options(link_audio_hut PRIVATE /experimental:c11atomics)
endif()

target_link_libraries(link_audio_hut abl_link)
