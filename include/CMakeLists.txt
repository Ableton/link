cmake_minimum_required(VERSION 3.0)
project(LinkCore)

#  ____
# / ___|  ___  _   _ _ __ ___ ___  ___
# \___ \ / _ \| | | | '__/ __/ _ \/ __|
#  ___) | (_) | |_| | | | (_|  __/\__ \
# |____/ \___/ \__,_|_|  \___\___||___/
#

set(link_core_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ableton/link)
set(link_core_HEADERS
  ${link_core_DIR}/Beats.hpp
  ${link_core_DIR}/ClientBeatTimeline.hpp
  ${link_core_DIR}/Controller.hpp
  ${link_core_DIR}/Gateway.hpp
  ${link_core_DIR}/GhostXForm.hpp
  ${link_core_DIR}/HostTimeUtility.hpp
  ${link_core_DIR}/Kalman.hpp
  ${link_core_DIR}/LinearRegression.hpp
  ${link_core_DIR}/Measurement.hpp
  ${link_core_DIR}/MeasurementEndpointV4.hpp
  ${link_core_DIR}/MeasurementService.hpp
  ${link_core_DIR}/NodeId.hpp
  ${link_core_DIR}/NodeState.hpp
  ${link_core_DIR}/PayloadEntries.hpp
  ${link_core_DIR}/Peers.hpp
  ${link_core_DIR}/PeerState.hpp
  ${link_core_DIR}/PingResponder.hpp
  ${link_core_DIR}/SessionId.hpp
  ${link_core_DIR}/Sessions.hpp
  ${link_core_DIR}/Tempo.hpp
  ${link_core_DIR}/Timeline.hpp
  ${link_core_DIR}/v1/Messages.hpp
  PARENT_SCOPE
)

set(link_discovery_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ableton/discovery)
set(link_discovery_HEADERS
  ${link_discovery_DIR}/InterfaceScanner.hpp
  ${link_discovery_DIR}/IpV4Interface.hpp
  ${link_discovery_DIR}/Messenger.hpp
  ${link_discovery_DIR}/NetworkByteStreamSerializable.hpp
  ${link_discovery_DIR}/Payload.hpp
  ${link_discovery_DIR}/PeerGateway.hpp
  ${link_discovery_DIR}/PeerGateways.hpp
  ${link_discovery_DIR}/PeerObserver.hpp
  ${link_discovery_DIR}/Service.hpp
  ${link_discovery_DIR}/Socket.hpp
  ${link_discovery_DIR}/UdpMessenger.hpp
  ${link_discovery_DIR}/asio/AsioService.hpp
  ${link_discovery_DIR}/asio/AsioTimer.hpp
  ${link_discovery_DIR}/asio/AsioWrapper.hpp
  ${link_discovery_DIR}/test/Interface.hpp
  ${link_discovery_DIR}/test/PayloadEntries.hpp
  ${link_discovery_DIR}/test/Socket.hpp
  ${link_discovery_DIR}/util/Injected.hpp
  ${link_discovery_DIR}/util/Log.hpp
  ${link_discovery_DIR}/util/PooledHandlerService.hpp
  ${link_discovery_DIR}/util/SafeAsyncHandler.hpp
  ${link_discovery_DIR}/util/test/IoService.hpp
  ${link_discovery_DIR}/util/test/Timer.hpp
  ${link_discovery_DIR}/util/Timer.hpp
  ${link_discovery_DIR}/util/Unwrap.hpp
  ${link_discovery_DIR}/v1/Messages.hpp
  PARENT_SCOPE
)

set(link_platform_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ableton/platforms)
set(link_platform_HEADERS)
if(UNIX)
  set(link_platform_HEADERS
    ${link_platform_HEADERS}
    ${link_platform_DIR}/posix/Posix.hpp
  )

  if(APPLE)
    set(link_platform_HEADERS
      ${link_platform_HEADERS}
      ${link_platform_DIR}/darwin/Clock.hpp
      ${link_platform_DIR}/darwin/Darwin.hpp
      ${link_platform_DIR}/darwin/Link.hpp
    )
  elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(link_platform_HEADERS
      ${link_platform_HEADERS}
      ${link_platform_DIR}/linux/Link.hpp
      ${link_platform_DIR}/linux/Linux.hpp
      ${link_platform_DIR}/stl/Clock.hpp
    )
  endif()
elseif(WIN32)
  set(link_platform_HEADERS
    ${link_platform_HEADERS}
    ${link_platform_DIR}/windows/Clock.hpp
    ${link_platform_DIR}/windows/Link.hpp
    ${link_platform_DIR}/windows/Winsocket.hpp
  )
endif()
set(link_platform_HEADERS
  ${link_platform_HEADERS}
  PARENT_SCOPE
)

set(link_test_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ableton/test)
set(link_test_HEADERS
  ${link_test_DIR}/CatchWrapper.hpp
  ${link_test_DIR}/Platform.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test/catch/catch.hpp
  PARENT_SCOPE
)
