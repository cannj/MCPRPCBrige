cmake_minimum_required(VERSION 3.14)
project(McpRpcBridge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项
option(MCP_RPC_BUILD_EXAMPLES "Build example applications" OFF)
option(MCP_RPC_BUILD_TESTS "Build tests" ON)

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(nlohmann_json REQUIRED)

# 添加子目录
add_subdirectory(src)

if(MCP_RPC_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(MCP_RPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
