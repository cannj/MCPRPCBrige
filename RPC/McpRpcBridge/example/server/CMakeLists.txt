# Example Server - 演示如何暴露 RPC 服务为 MCP 工具

add_executable(mcp_example_server
    main.cc
)

target_link_libraries(mcp_example_server
    PRIVATE
        mcp_rpc_bridge
        krpc_bridge_proto  # 示例 proto 库
)

target_include_directories(mcp_example_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
