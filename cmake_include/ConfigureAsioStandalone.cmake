function(ConfigureAsioStandalone PATH_TO_LINK)

  set(asio_INCLUDE_DIR ${PATH_TO_LINK}/modules/asio-standalone/include)

  add_library(AsioStandalone::AsioStandalone IMPORTED INTERFACE)

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
    ASIO_VERSION_NAMESPACE=link_asio_1_38_2
  )

endfunction()
