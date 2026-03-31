# MCP-RPC Bridge 核心库

set(MCP_RPC_SOURCES
    mcp/ToolRegistry.cc
    mcp/ProtoSchemaConverter.cc
    mcp/ArgumentsDeserializer.cc
    mcp/MCPSession.cc
    mcp/StreamingResponseAggregator.cc
    invoker/RpcInvoker.cc
    invoker/KrpcInvoker.cc
    invoker/MockRpcInvoker.cc
)

set(MCP_RPC_HEADERS
    mcp/LazyValue.h
    mcp/ToolRegistry.h
    mcp/ProtoSchemaConverter.h
    mcp/ArgumentsDeserializer.h
    mcp/MCPSession.h
    mcp/StreamingConfig.h
    mcp/StreamingResponseAggregator.h
    invoker/RpcInvoker.h
    invoker/KrpcInvoker.h
    invoker/MockRpcInvoker.h
    McpRpcBridge.h
)

# 添加 Krpc 库
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../Krpc ${CMAKE_CURRENT_BINARY_DIR}/Krpc)

# 创建静态库
add_library(mcp_rpc_bridge STATIC ${MCP_RPC_SOURCES} ${MCP_RPC_HEADERS})

target_include_directories(mcp_rpc_bridge
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Protobuf_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Krpc/src/include
)

target_include_directories(mcp_rpc_bridge
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mcp_rpc_bridge
    PUBLIC
        nlohmann_json::nlohmann_json
        ${Protobuf_LIBRARIES}
    PRIVATE
        krpc_core
        glog
        zookeeper_mt
)

# 安装规则
install(TARGETS mcp_rpc_bridge
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mcp
    DESTINATION include/mcp_rpc_bridge
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/invoker
    DESTINATION include/mcp_rpc_bridge
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/McpRpcBridge.h
    DESTINATION include/mcp_rpc_bridge
)
