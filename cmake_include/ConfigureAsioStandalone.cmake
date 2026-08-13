function(ConfigureAsioStandalone PATH_TO_LINK)

  option(LINK_USE_BUNDLED_ASIO "Use Link's bundled asio standalone library" ON)

  add_library(AsioStandalone::AsioStandalone IMPORTED INTERFACE)

  if(NOT LINK_USE_BUNDLED_ASIO)
    return()
  endif()

  set(asio_INCLUDE_DIR ${PATH_TO_LINK}/modules/asio-standalone/include)

  # Derive the inline namespace name from the bundled Asio version
  if(NOT EXISTS ${asio_INCLUDE_DIR}/asio/version.hpp)
    message(FATAL_ERROR
      "${asio_INCLUDE_DIR}/asio/version.hpp not found. Did you forget to run "
      "'git submodule update --init --recursive'?")
  endif()
  file(STRINGS ${asio_INCLUDE_DIR}/asio/version.hpp asio_VERSION_LINE
    REGEX "^#define ASIO_VERSION ")
  string(REGEX MATCH "[0-9]+" asio_VERSION "${asio_VERSION_LINE}")
  if(NOT asio_VERSION MATCHES "^[0-9]+$")
    message(FATAL_ERROR
      "Could not parse ASIO_VERSION from ${asio_INCLUDE_DIR}/asio/version.hpp. "
      "Its format may have changed; update ConfigureAsioStandalone.cmake.")
  endif()
  math(EXPR asio_VERSION_MAJOR "${asio_VERSION} / 100000")
  math(EXPR asio_VERSION_MINOR "${asio_VERSION} / 100 % 1000")
  math(EXPR asio_VERSION_SUB "${asio_VERSION} % 100")
  set(asio_NAMESPACE
    link_asio_${asio_VERSION_MAJOR}_${asio_VERSION_MINOR}_${asio_VERSION_SUB})
  message(STATUS "Link: using bundled Asio with ASIO_VERSION_NAMESPACE=${asio_NAMESPACE}")

  set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES
    ${asio_INCLUDE_DIR}
  )
  set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
    ${asio_INCLUDE_DIR}
  )

  set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS
    ASIO_NO_TYPEID=1
    ASIO_STANDALONE=1
    ASIO_VERSION_NAMESPACE=${asio_NAMESPACE}
  )

endfunction()
