cmake_minimum_required(VERSION 3.5)
project(mind_ctrl)

set(CMAKE_CXX_STANDARD 11)

# Output directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Find Thrift
include_directories("/usr/local/lib/")

find_program(THRIFT_COMPILER thrift)
if (NOT THRIFT_COMPILER)
  message(FATAL_ERROR "Thrift compiler not found! Make sure it's installed and available in PATH.")
endif()

# Include Thrift headers
# include_directories(${THRIFT_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/mind_ctrl_server/gen-cpp)
message(STATUS "Thrift include dir: ${THRIFT_INCLUDE_DIR}")
message(STATUS "Cmake src dir: ${CMAKE_SOURCE_DIR}/mind_ctrl_server/gen-cpp")

add_subdirectory(mind_ctrl_thrift)
add_subdirectory(mind_ctrl_server)
add_subdirectory(libuser)
add_subdirectory(tna_disagg_switch_base)
