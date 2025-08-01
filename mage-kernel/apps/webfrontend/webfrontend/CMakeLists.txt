project(hashtable CXX)
cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_STANDARD 17)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
add_compile_options(
	-static
	-static-libgcc
	-static-libstdc++
)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive")


set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(include)
include_directories(snappy)
include_directories(snappy/build)

link_directories(snappy/build)

add_library(local_concurrent_hopscotch src/local_concurrent_hopscotch.cpp src/spinlock.cpp  src/MurmurHash3.cpp src/slab.cpp)
target_compile_options(local_concurrent_hopscotch PRIVATE -fpermissive)
target_compile_options(local_concurrent_hopscotch PRIVATE -msse3)
target_compile_options(local_concurrent_hopscotch PRIVATE -g)
target_compile_options(local_concurrent_hopscotch PRIVATE -ggdb)

add_executable(web_frontend  web_frontend.cpp)
target_link_libraries(web_frontend PRIVATE local_concurrent_hopscotch)
target_link_libraries(web_frontend PRIVATE snappy)
