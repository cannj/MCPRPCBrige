# Example 示例应用

option(MCP_RPC_BUILD_EXAMPLE_SERVER "Build example server" ON)
option(MCP_RPC_BUILD_EXAMPLE_CLIENT "Build example client" ON)

if(MCP_RPC_BUILD_EXAMPLE_SERVER)
    add_subdirectory(server)
endif()

if(MCP_RPC_BUILD_EXAMPLE_CLIENT)
    add_subdirectory(client)
endif()
