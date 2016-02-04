cmake_minimum_required(VERSION 3.0)
project(Link)

#   ___        _   _
#  / _ \ _ __ | |_(_) ___  _ __  ___
# | | | | '_ \| __| |/ _ \| '_ \/ __|
# | |_| | |_) | |_| | (_) | | | \__ \
#  \___/| .__/ \__|_|\___/|_| |_|___/
#       |_|

# Note: Please use the LINK_* prefix for all project-specific options

if(UNIX)
  set(LINK_WORD_SIZE "64" CACHE STRING "Set the word size (must be either 32 or 64)")
  option(LINK_ENABLE_ASAN "Build with Address Sanitizier (ASan)" OFF)
endif()

if(WIN32)
  option(LINK_BUILD_MP "Build with multiple processes (/MP)" ON)
  option(LINK_BUILD_SHARED_LIBS "Build shared libraries (DLLs)" OFF)
  option(LINK_USE_STATIC_RT "Use static runtime library" ON)
endif()

# Other CMake files must be included only after declaring build options
include(cmake_include/compile-flags.cmake)
include(cmake_include/platform.cmake)
include(cmake_include/word-size.cmake)

#  ____       _   _
# |  _ \ __ _| |_| |__  ___
# | |_) / _` | __| '_ \/ __|
# |  __/ (_| | |_| | | \__ \
# |_|   \__,_|\__|_| |_|___/
#

include_directories(include)
include_directories(modules/asio-standalone/asio/include)

add_subdirectory(include)
add_subdirectory(src)

#  ____
# / ___| _   _ _ __ ___  _ __ ___   __ _ _ __ _   _
# \___ \| | | | '_ ` _ \| '_ ` _ \ / _` | '__| | | |
#  ___) | |_| | | | | | | | | | | | (_| | |  | |_| |
# |____/ \__,_|_| |_| |_|_| |_| |_|\__,_|_|   \__, |
#                                             |___/

message("-- Build options")

get_cmake_property(all_variables VARIABLES)
string(REGEX MATCHALL "(^|;)LINK_[A-Z_]+" link_variables "${all_variables}")
foreach(variable ${link_variables})
  message("   ${variable}: ${${variable}}")
endforeach()

message("-- Build configuration")

if(CMAKE_BUILD_TYPE)
  message("   Build type: ${CMAKE_BUILD_TYPE}")
else()
  message("   Build type: Set by IDE")
endif()

get_directory_property(preprocessor_flags COMPILE_DEFINITIONS)
message("   Preprocessor flags: ${preprocessor_flags}")

message("   C Compiler: ${CMAKE_C_COMPILER}")
message("   C Compiler flags: ${CMAKE_C_FLAGS}")
message("   C Compiler flags (Debug): ${CMAKE_C_FLAGS_DEBUG}")
message("   C Compiler flags (Release): ${CMAKE_C_FLAGS_RELEASE}")
message("   C++ Compiler: ${CMAKE_CXX_COMPILER}")
message("   C++ Compiler flags: ${CMAKE_CXX_FLAGS}")
message("   C++ Compiler flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message("   C++ Compiler flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
message("   Linker: ${CMAKE_LINKER}")
message("   Linker flags: ${CMAKE_SHARED_LINKER_FLAGS}")
message("   Linker flags (Debug): ${CMAKE_SHARED_LINKER_FLAGS_DEBUG}")
message("   Linker flags (Release): ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
