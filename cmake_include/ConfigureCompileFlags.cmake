cmake_minimum_required(VERSION 3.10)

set(build_flags_COMMON_LIST)
set(build_flags_DEBUG_LIST)
set(build_flags_RELEASE_LIST)

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
  if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-Weverything"
      "-Werror"
      "-Wno-c++98-compat"
      "-Wno-c++98-compat-pedantic"
      "-Wno-deprecated"
      "-Wno-disabled-macro-expansion"
      "-Wno-exit-time-destructors"
      "-Wno-padded"
      "-Wno-poison-system-directories"
      "-Wno-reserved-id-macro"
      "-Wno-unknown-warning-option"
      "-Wno-unsafe-buffer-usage"
      "-Wno-unused-member-function"
    )

  # GCC-specific flags
  # Unfortunately, we can't use -Werror on GCC, since there is no way to suppress the
  # warnings generated by -fpermissive.
  elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "-Werror"
      "-Wno-multichar"
    )
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
  add_definitions("/D_SCL_SECURE_NO_WARNINGS")
  if(LINK_BUILD_VLD)
    add_definitions("/DLINK_BUILD_VLD=1")
  else()
    add_definitions("/DLINK_BUILD_VLD=0")
  endif()
  if(LINK_WINDOWS_SETTHREADDESCRIPTION)
    add_definitions("/DLINK_WINDOWS_SETTHREADDESCRIPTION")
  endif()

  set(build_flags_DEBUG_LIST
    "/DDEBUG=1"
  )
  set(build_flags_RELEASE_LIST
    "/DNDEBUG=1"
  )

  set(build_flags_COMMON_LIST
    ${build_flags_COMMON_LIST}
    "/MP"
    "/Wall"
    "/WX"
    "/EHsc"

    #############################
    # Ignored compiler warnings #
    #############################
    "/wd4061" # Enumerator 'identifier' in switch of enum 'enumeration' is not explicitly handled by a case label
    "/wd4265" # 'Class' : class has virtual functions, but destructor is not virtual
    "/wd4350" # Behavior change: 'member1' called instead of 'member2'
    "/wd4355" # 'This' : used in base member initializer list
    "/wd4365" # 'Action': conversion from 'type_1' to 'type_2', signed/unsigned mismatch
    "/wd4371" # Layout of class may have changed from a previous version of the compiler due to better packing of member 'member'
    "/wd4503" # 'Identifier': decorated name length exceeded, name was truncated
    "/wd4510" # 'Class': default constructor could not be generated
    "/wd4512" # 'Class': assignment operator could not be generated
    "/wd4514" # 'Function' : unreferenced inline function has been removed
    "/wd4571" # Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
    "/wd4610" # 'Class': can never be instantiated - user defined constructor required
    "/wd4625" # 'Derived class' : copy constructor was implicitly defined as deleted because a base class copy constructor is inaccessible or deleted
    "/wd4626" # 'Derived class' : assignment operator was implicitly defined as deleted because a base class assignment operator is inaccessible or deleted
    "/wd4628" # digraphs not supported with -Ze. Character sequence 'digraph' not interpreted as alternate token for 'char'
    "/wd4640" # 'Instance': construction of local static object is not thread-safe
    "/wd4710" # 'Function': function not inlined
    "/wd4711" # Function 'function' selected for inline expansion
    "/wd4723" # potential divide by 0
    "/wd4738" # Storing 32-bit float result in memory, possible loss of performance
    "/wd4820" # 'Bytes': bytes padding added after construct 'member_name'
    "/wd4996" # Your code uses a function, class member, variable, or typedef that's marked deprecated
    "/wd5045" # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    "/wd5204" # 'class' : class has virtual functions, but destructor is not virtual
  )

  if(MSVC_VERSION VERSION_GREATER 1800)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "/wd4464" # Relative include path contains '..'
      "/wd4548" # Expression before comma has no effect; expected expression with side-effect
      "/wd4623" # 'Derived class': default constructor could not be generated because a base class default constructor is inaccessible
      "/wd4868" # Compiler may not enforce left-to-right evaluation order in braced initializer list
      "/wd5026" # Move constructor was implicitly defined as deleted
      "/wd5027" # Move assignment operator was implicitly defined as deleted
      "/wd5262" # implicit fall-through
      "/wd5264" # 'variable-name': 'const' variable is not used
    )
  endif()

  if(MSVC_VERSION VERSION_GREATER 1900)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "/wd4987" # nonstandard extension used: 'throw (...)'
      "/wd4774" # 'printf_s' : format string expected in argument 1 is not a string literal
      "/wd5039" # "pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception."
    )
  endif()

  if(NOT LINK_BUILD_ASIO)
    set(build_flags_COMMON_LIST
      ${build_flags_COMMON_LIST}
      "/wd4917" # 'Symbol': a GUID can only be associated with a class, interface or namespace
                # This compiler warning is generated by Microsoft's own ocidl.h, which is
                # used when building the WASAPI driver.
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
