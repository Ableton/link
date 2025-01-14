function(ConfigureAsioStandalone PATH_TO_LINK)

  add_library(AsioStandalone::AsioStandalone IMPORTED INTERFACE)

  set_property(TARGET AsioStandalone::AsioStandalone APPEND PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES
    ${PATH_TO_LINK}/modules/asio-standalone/asio/include
  )

endfunction()
