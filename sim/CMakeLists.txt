cmake_minimum_required(VERSION 3.25)
project(radio_sim C)

include(../../CompFlags.cmake)

# Find ZeroMQ dependency
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

# Determine whether to include simulith source files in component targets.
if(EXISTS "${SIMULITH_BUILD_LIB}")
    set(SIMULITH_SRC_FILES "")
else()
    set(SIMULITH_SRC_FILES
    ${SIMULITH_DIR}/src/simulith_common.c
    ${SIMULITH_DIR}/src/simulith_uart.c
    )
endif()

# Create the shared library for dynamic loading by director
add_library(radio_sim_component SHARED
    radio_sim.c
    ../shared/radio_device.c
    ../../../fsw/apps/hwlib/sim/src/libuart.c
    ${SIMULITH_SRC_FILES}
)

# Include directories
target_include_directories(radio_sim_component
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SIMULITH_DIR}/include
    ../shared
    ../../../fsw/apps/hwlib/fsw/public_inc
    ${ZeroMQ_INCLUDE_DIRS}
)

# Link against Simulith and ZeroMQ
target_link_libraries(radio_sim_component
    PRIVATE
    simulith
    ${ZeroMQ_LIBRARIES}
)

# Set C standard for the component
set_target_properties(radio_sim_component PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    OUTPUT_NAME "radio_sim"
    PREFIX ""
)

# Add dependencies on simulith for both targets
add_dependencies(radio_sim_component simulith)
