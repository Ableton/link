add_library(AsioStandalone::AsioStandalone IMPORTED INTERFACE)

set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
  INTERFACE_INCLUDE_DIRECTORIES
  ${CMAKE_CURRENT_LIST_DIR}/../modules/asio-standalone/asio/include
)

if (WIN32)
  set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
    INTERFACE_COMPILE_DEFINITIONS
    _WIN32_WINNT=0x0501
    _SCL_SECURE_NO_WARNINGS
  )
endif()
