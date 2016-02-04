cmake_minimum_required(VERSION 3.0)
project(LinkTest)

#  ____
# / ___|  ___  _   _ _ __ ___ ___  ___
# \___ \ / _ \| | | | '__/ __/ _ \/ __|
#  ___) | (_) | |_| | | | (_|  __/\__ \
# |____/ \___/ \__,_|_|  \___\___||___/
#

set(link_discovery_test_SOURCES
  ableton/discovery/tst_Payload.cpp
  ableton/discovery/tst_PeerGateway.cpp
  ableton/discovery/tst_PeerGateways.cpp
  ableton/discovery/tst_UdpMessenger.cpp
  ableton/discovery/v1/tst_Messages.cpp
)

set(link_core_test_SOURCES
  ableton/link/tst_Beats.cpp
  ableton/link/tst_ClientBeatTimeline.cpp
  ableton/link/tst_Controller.cpp
  ableton/link/tst_HostTimeUtility.cpp
  ableton/link/tst_InterfaceScanner.cpp
  ableton/link/tst_Kalman.cpp
  ableton/link/tst_LinearRegression.cpp
  ableton/link/tst_Measurement.cpp
  ableton/link/tst_Peers.cpp
  ableton/link/tst_PingResponder.cpp
  ableton/link/tst_Tempo.cpp
  ableton/link/tst_Timeline.cpp
)


set(link_platform_SOURCES)
if(UNIX)
  set(link_platform_SOURCES
    ${link_platform_SOURCES}
    ableton/platform/Posix.cpp
  )
elseif(WIN32)
  set(link_platform_SOURCES
    ${link_platform_SOURCES}
    ableton/platform/Winsocket.cpp
  )
endif()

set(link_test_SOURCES
  test/catch/CatchMain.cpp
)

source_group("Link" FILES ${link_core_HEADERS})
source_group("Discovery" FILES ${link_discovery_HEADERS})
source_group("Platform" FILES
  ${link_platform_HEADERS}
  ${link_platform_SOURCES}
)
source_group("Tests" FILES
  ${link_core_test_SOURCES}
  ${link_discovery_test_SOURCES}
)
source_group("Test Utils" FILES
  ${link_test_HEADERS}
  ${link_test_SOURCES}
)

#  _____                    _
# |_   _|_ _ _ __ __ _  ___| |_  ___
#   | |/ _` | '__/ _` |/ _ \ __|/ __|
#   | | (_| | | | (_| |  __/ |_ \__ \
#   |_|\__,_|_|  \__, |\___|\__||___/
#                |___/

add_executable(LinkCoreTest
  ${link_core_HEADERS}
  ${link_discovery_HEADERS}
  ${link_platform_HEADERS}
  ${link_test_HEADERS}

  ${link_core_test_SOURCES}
  ${link_platform_SOURCES}
  ${link_test_SOURCES}
)

# Force C++11 support
target_compile_features(LinkCoreTest PRIVATE
  cxx_generalized_initializers
)
target_set_word_size(LinkCoreTest)

add_executable(LinkDiscoveryTest
  ${link_discovery_HEADERS}
  ${link_platform_HEADERS}
  ${link_test_HEADERS}

  ${link_discovery_test_SOURCES}
  ${link_platform_SOURCES}
  ${link_test_SOURCES}
)

# Force C++11 support
target_compile_features(LinkDiscoveryTest PRIVATE
  cxx_generalized_initializers
)
target_set_word_size(LinkDiscoveryTest)
