cmake_minimum_required(VERSION 3.0)

set(build_flags_COMMON_LIST)
set(build_flags_DEBUG_LIST)
set(build_flags_RELEASE_LIST)

add_definitions("-DASIO_STANDALONE=1")

#  _   _       _
# | | | |_ __ (_)_  __
# | | | | '_ \| \ \/ /
# | |_| | | | | |>  <
#  \___/|_| |_|_/_/\_\
#

if(UNIX)
  # Common flags for all Unix compilers
  set(build_flags_DEBUG_LIST
    "-DDEBUG=1"
  )
  set(build_flags_RELEASE_LIST
    "-DNDEBUG=1"
  )

  # Clang-specific flags
  if(${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-Wno-global-constructors"
      "-Wno-deprecated"
      "-Wno-over-aligned"
    )

  # GCC-specific flags
  elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-fpermissive"
    )
  endif()

  # Set word size
  if(${LINK_WORD_SIZE} EQUAL 32)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-m32"
    )
  elseif(${LINK_WORD_SIZE} EQUAL 64)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-m64"
    )
  else()
    message(FATAL_ERROR "Invalid word size '${LINK_WORD_SIZE}', must be either 32 or 64.")
  endif()

  # ASan support
  if(LINK_ENABLE_ASAN)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-fsanitize=address"
      "-fno-omit-frame-pointer"
    )
  endif()

# __        ___           _
# \ \      / (_)_ __   __| | _____      _____
#  \ \ /\ / /| | '_ \ / _` |/ _ \ \ /\ / / __|
#   \ V  V / | | | | | (_| | (_) \ V  V /\__ \
#    \_/\_/  |_|_| |_|\__,_|\___/ \_/\_/ |___/
#

elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
  add_definitions("/D_WIN32_WINNT=0x0501")
  add_definitions("/DINCL_EXTRA_HTON_FUNCTIONS")
  add_definitions("/D_SCL_SECURE_NO_WARNINGS")

  set(build_flags_DEBUG_LIST
    "/DDEBUG=1"
  )
  set(build_flags_RELEASE_LIST
    "/DNDEBUG=1"
  )

  # Disabled compiler warnings that should be ignored project-wide
  set(build_flags_COMMON_LIST
    ${build_flags_COMMON_LIST}
    # 'Identifier': decorated name length exceeded, name was truncated
    "/wd4503"
  )

  if(LINK_BUILD_MP)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "/MP"
    )
  endif()

  if(LINK_USE_STATIC_RT)
    # CMake really likes /MD by default, so replace it with /MT
    foreach(flag_var CMAKE_CXX_FLAGS_DEBUG)
      if(${flag_var} MATCHES "/MDd")
        string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()
    foreach(flag_var CMAKE_CXX_FLAGS_RELEASE)
      if(${flag_var} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()
  else()
    set(build_flags_DEBUG_LIST
      ${build_flags_DEBUG_LIST}
      "/MDd"
    )
    set(build_flags_RELEASE_LIST
      ${build_flags_RELEASE_LIST}
      "/MD"
    )
  endif()
endif()

#  ____       _      __ _
# / ___|  ___| |_   / _| | __ _  __ _ ___
# \___ \ / _ \ __| | |_| |/ _` |/ _` / __|
#  ___) |  __/ |_  |  _| | (_| | (_| \__ \
# |____/ \___|\__| |_| |_|\__,_|\__, |___/
#                               |___/

# Translate lists to strings
string(REPLACE ";" " " build_flags_COMMON "${build_flags_COMMON_LIST}")
string(REPLACE ";" " " build_flags_DEBUG "${build_flags_DEBUG_LIST}")
string(REPLACE ";" " " build_flags_RELEASE "${build_flags_RELEASE_LIST}")

# Set flags for different build types
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${build_flags_COMMON}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${build_flags_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${build_flags_RELEASE}")
