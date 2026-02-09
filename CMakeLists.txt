cmake_minimum_required(VERSION 3.16)
project(opcua_multi_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_EXAMPLE "Build example executable" ON)

find_path(OPEN62541_INCLUDE_DIR
    NAMES open62541/client.h
    HINTS /usr/local/include /usr/include
)

find_library(OPEN62541_LIBRARY
    NAMES open62541 libopen62541
    HINTS /usr/local/lib /usr/lib /usr/lib64
)

if(NOT OPEN62541_INCLUDE_DIR OR NOT OPEN62541_LIBRARY)
    message(FATAL_ERROR "open62541 not found. Please ensure headers and static/shared library are installed.")
endif()

add_library(opcua_client_core
    src/OpcUaMultiClientManager.cpp
    src/OpcUaSession.cpp
)

target_include_directories(opcua_client_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPEN62541_INCLUDE_DIR}
)

target_link_libraries(opcua_client_core
    PUBLIC
        ${OPEN62541_LIBRARY}
)

if(BUILD_EXAMPLE)
    add_executable(opcua_client_example
        src/main.cpp
    )
    target_link_libraries(opcua_client_example PRIVATE opcua_client_core)
endif()
