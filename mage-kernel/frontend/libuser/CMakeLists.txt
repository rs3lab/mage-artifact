cmake_minimum_required(VERSION 3.5)
project(libuser)

set(TESTDATADIR "")
set(CMAKE_INSTALL_RPATH $ORIGIN/../lib)
set(PROGRAM_NAME libuser)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -lpthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-stringop-overflow -lpthread")

add_subdirectory(thrift)
add_subdirectory(librpc)
