# Define source files
set(SERVER_SRC
    main.cpp
    server.cpp
    gen-cpp/MindCtrl.cpp
    # gen-cpp/string_exchange_constants.cpp
    gen-cpp/mind_ctrl_types.cpp
)

# Define the executable and link the libraries
add_executable(server ${SERVER_SRC})
target_link_libraries(server thrift)