# Test 测试应用

enable_testing()

# 查找 Protobuf 包
find_package(Protobuf REQUIRED)

# 生成测试用的 proto 文件
protobuf_generate_cpp(TEST_PROTO_SRCS TEST_PROTO_HDRS test_service.proto)

# 添加 proto 库
add_library(test_proto_lib STATIC ${TEST_PROTO_SRCS})
target_include_directories(test_proto_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${Protobuf_INCLUDE_DIRS})

# ============================================
# LazyValue 测试
# ============================================

# 基础测试（不依赖 proto）
add_executable(test_lazy_value test_lazy_value.cc)
target_link_libraries(test_lazy_value PRIVATE mcp_rpc_bridge)
add_test(NAME LazyValueTest COMMAND test_lazy_value)

# 完整测试（不依赖 proto）
add_executable(test_lazy_value_complete test_lazy_value_complete.cc)
target_link_libraries(test_lazy_value_complete PRIVATE mcp_rpc_bridge)
add_test(NAME LazyValueCompleteTest COMMAND test_lazy_value_complete)

# ============================================
# ToolRegistry 测试
# ============================================

# 基础测试（不依赖 proto）
add_executable(test_tool_registry test_tool_registry.cc)
target_link_libraries(test_tool_registry PRIVATE mcp_rpc_bridge test_proto_lib)
add_test(NAME ToolRegistryTest COMMAND test_tool_registry)

# 完整测试（依赖 proto）
add_executable(test_tool_registry_complete
    test_tool_registry_complete.cc
    ${TEST_PROTO_SRCS}
)
target_include_directories(test_tool_registry_complete
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
)
target_link_libraries(test_tool_registry_complete
    PRIVATE
        mcp_rpc_bridge
        test_proto_lib
        ${Protobuf_LIBRARIES}
)
add_test(NAME ToolRegistryCompleteTest COMMAND test_tool_registry_complete)

# ============================================
# ProtoSchemaConverter 测试
# ============================================

add_executable(test_schema_converter test_schema_converter.cc)
target_link_libraries(test_schema_converter PRIVATE mcp_rpc_bridge test_proto_lib)
add_test(NAME SchemaConverterTest COMMAND test_schema_converter)

# ============================================
# MCPSession 测试
# ============================================

add_executable(test_mcp_session test_mcp_session.cc)
target_link_libraries(test_mcp_session PRIVATE mcp_rpc_bridge)
add_test(NAME McpSessionTest COMMAND test_mcp_session)
