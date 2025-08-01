cmake_minimum_required(VERSION 3.5)
project(tna_disagg_switch_base)

set(TESTDATADIR "")
set(CMAKE_INSTALL_RPATH $ORIGIN/../lib)
set(PROGRAM_NAME tna_disagg_switch_base)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../mind_linux)
include_directories(/usr/local/include/hiredis/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libuser/thrift/src)

set(LIBUSER_PATH "../libuser/librpc")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -lpthread -lboost_system -pthread -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-stringop-overflow -lpthread -g")
add_executable(${PROGRAM_NAME}
  disagg_switch.cpp disagg_bf_client.cpp disagg_switch_config.cpp disagg_pkt_record.cpp
  controller/controller.c controller/cacheline_manager.c controller/cache_manager_thread.c
  controller/ht_body.c controller/task_management.c controller/request_handler.c
  controller/memory_management.c controller/mmap_ftns.c controller/mmap_unmapped_area.c controller/rbtree_lib.c controller/rbtree_ftns.c
  controller/memory_node_connect.c controller/controller_tests.c controller/debug.c controller/remote_thread_management.c
  controller/kshmem.c controller/pid_ns.c
  controller/MindAccess.c
  gen-cpp/CacheService.cpp gen-cpp/mind_bf_types.cpp

  # redis logger
  redis_logger/redis_logger.c

  # user library for handling rack-scale shell requests
  ${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src/MindUserServer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src/MindUserServer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src/MindUser.h ${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src/MindUserClient.h ${CMAKE_CURRENT_SOURCE_DIR}/../libuser/librpc/src/MindUser.cpp
)

target_link_libraries(${PROGRAM_NAME} PUBLIC hiredis thrift)
install(TARGETS ${PROGRAM_NAME} DESTINATION bin)
