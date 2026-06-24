cmake_minimum_required(VERSION 3.25)
project(iris VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(CLANG_TIDY NAMES clang-tidy-19 clang-tidy REQUIRED)
set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
message(STATUS "clang-tidy enabled: ${CLANG_TIDY}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

# Common warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Common library (shared code between client and server)
add_library(iris_common STATIC
    src/common/socket.cpp
    src/common/protocol.cpp
)
target_include_directories(iris_common PUBLIC include include/common)

# Server
add_executable(iris_server
    src/server/main.cpp
    src/server/server.cpp
    src/server/chat.cpp
)
target_link_libraries(iris_server PRIVATE iris_common Threads::Threads)
target_include_directories(iris_server PRIVATE include include/server)

# Client
add_executable(iris_client
    src/client/main.cpp
    src/client/client.cpp
)
target_link_libraries(iris_client PRIVATE iris_common Threads::Threads)
target_include_directories(iris_client PRIVATE include include/client)
