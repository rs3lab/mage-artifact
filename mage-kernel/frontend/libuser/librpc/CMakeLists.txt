include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(testUser
        src/testUser.cpp
        src/MindUser.h
        src/MindUserClient.h
        src/MindUser.cpp)

target_link_libraries(testUser PUBLIC event thrift)

