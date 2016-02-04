cmake_minimum_required(VERSION 3.0)

if(APPLE)
  add_definitions("-DPLATFORM_MACOSX=1")
elseif(WIN32)
  add_definitions("/DPLATFORM_WINDOWS=1")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  add_definitions("-DPLATFORM_LINUX=1")
endif()
