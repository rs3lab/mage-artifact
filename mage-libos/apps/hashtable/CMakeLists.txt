project(hashtable CXX)
cmake_minimum_required(VERSION 3.20)

set (CMAKE_CXX_STANDARD 17)
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
