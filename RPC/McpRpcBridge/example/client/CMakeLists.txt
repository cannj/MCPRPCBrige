# Example Client - 演示如何调用 MCP 服务

add_executable(mcp_example_client
    main.cc
)

target_link_libraries(mcp_example_client
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_include_directories(mcp_example_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
