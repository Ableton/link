cmake_minimum_required(VERSION 3.10)

function(ConfigureAbletonLink PATH_TO_LINK)
  add_library(Ableton::Link IMPORTED INTERFACE)
  set_property(TARGET Ableton::Link APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES
    ${PATH_TO_LINK}/include
  )

  # Force C++11 support for consuming targets
  set_property(TARGET Ableton::Link APPEND PROPERTY
    INTERFACE_COMPILE_FEATURES
    cxx_generalized_initializers
  )

  if(UNIX)
    set_property(TARGET Ableton::Link APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS
      LINK_PLATFORM_UNIX=1
    )
  endif()

  if(APPLE)
    set_property(TARGET Ableton::Link APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS
      LINK_PLATFORM_MACOSX=1
  )
  elseif(WIN32)
    set_property(TARGET Ableton::Link APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS
      LINK_PLATFORM_WINDOWS=1
    )
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD|GNU")
      set_property(TARGET Ableton::Link APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS
        LINK_PLATFORM_LINUX=1
    )
    set_property(TARGET Ableton::Link APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES
      atomic
      pthread
    )
  endif()

  include(${PATH_TO_LINK}/cmake_include/ConfigureAsioStandalone.cmake)
    ConfigureAsioStandalone(${PATH_TO_LINK})
    set_property(TARGET Ableton::Link APPEND PROPERTY
      INTERFACE_LINK_LIBRARIES
      AsioStandalone::AsioStandalone
    )

  set_property(TARGET Ableton::Link APPEND PROPERTY
    INTERFACE_SOURCES
    ${PATH_TO_LINK}/include/ableton/Link.hpp
  )
endfunction()
