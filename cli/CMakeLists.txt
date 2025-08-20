cmake_minimum_required(VERSION 3.25)
project (radio_cli)

if (NOT DEFINED TGTNAME)
  message(FATAL_ERROR "TGTNAME must be defined on the cmake command line (e.g. \"-DTGTNAME=cpu1\")")
endif()

include(../../CompFlags.cmake)

if(${TGTNAME} STREQUAL cpu1)
  # Find ZeroMQ via pkg-config
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZeroMQ REQUIRED libzmq)
  
  # Find Simulith headers
  set(SIMULITH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../simulith" CACHE PATH "Path to Simulith directory")
  if(NOT EXISTS "${SIMULITH_DIR}/include/simulith_uart.h")
      message(FATAL_ERROR "Could not find simulith_uart.h in ${SIMULITH_DIR}/include. Please set SIMULITH_DIR to the correct path.")
  endif()

  # Prefer a pre-built simulith static library if present in the simulith build dir.
  set(SIMULITH_BUILD_LIB "${SIMULITH_DIR}/build/libsimulith.a")
  if(EXISTS "${SIMULITH_BUILD_LIB}")
      message(STATUS "Using prebuilt simulith: ${SIMULITH_BUILD_LIB}")
      add_library(simulith STATIC IMPORTED)
      set_target_properties(simulith PROPERTIES IMPORTED_LOCATION "${SIMULITH_BUILD_LIB}")
  else()
      # Only add Simulith subdirectory if not already added
      if(NOT TARGET simulith)
          add_subdirectory(${SIMULITH_DIR} ${CMAKE_BINARY_DIR}/simulith)
      endif()
  endif()
endif()


# Use target_include_directories for radio_cli
set(radio_cli_includes
    "./"
    "../fsw/platform_inc"
    "../fsw/src"
    "../shared"
    "../../../fsw/apps/hwlib/fsw/public_inc"
)

set(radio_cli_src
  radio_cli.c 
  ../shared/radio_device.c
)

# Hardware library switch based on TGTNAME
if(${TGTNAME} STREQUAL cpu1)
  list(APPEND radio_cli_includes 
    "${SIMULITH_DIR}/include" 
    "../../../fsw/apps/hwlib/sim/inc"
  )
  set(radio_cli_src 
    ${radio_cli_src}
    ${SIMULITH_DIR}/src/simulith_common.c
    ${SIMULITH_DIR}/src/simulith_uart.c
    ../../../fsw/apps/hwlib/sim/src/libuart.c
  )
  set(radio_cli_libs
    ${ZeroMQ_LIBRARIES}
    simulith
  )
endif()
if(${TGTNAME} STREQUAL cpu2)
  set(radio_cli_src 
    ${radio_cli_src}
    ../../../fsw/apps/hwlib/fsw/linux/libuart.c
  )
endif()


add_executable(radio_cli ${radio_cli_src})
target_include_directories(radio_cli PRIVATE ${radio_cli_includes})
target_link_libraries(radio_cli ${radio_cli_libs})

if(${TGTNAME} STREQUAL cpu1)
  set_target_properties(radio_cli PROPERTIES COMPILE_FLAGS "-g" LINK_FLAGS "-g")
endif()
