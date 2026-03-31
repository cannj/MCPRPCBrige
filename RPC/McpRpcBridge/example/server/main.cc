/**
 * @file main.cc
 * @brief MCP-RPC Bridge 示例服务器
 *
 * 演示如何将现有的 Krpc 服务暴露为 MCP 工具。
 */

#include "McpRpcBridge.h"
#include <iostream>
#include <string>

// 前置声明示例服务（由 protobuf 生成）
// namespace example {
//     class UserServiceRpc;
// }

int main(int argc, char* argv[]) {
    std::cout << "MCP-RPC Bridge Example Server" << std::endl;
    std::cout << "Version: " << mcp_rpc::GetVersionString() << std::endl;

    // ========================================================================
    // 步骤 1: 创建并注册 RPC 服务
    // ========================================================================

    // 创建工具注册表
    mcp_rpc::ToolRegistry registry;

    // TODO: 创建实际的 RPC 服务对象
    // example::UserServiceRpc* user_service = new example::UserServiceRpc();

    // 注册服务（自动注册所有方法）
    // 需要获取 ServiceDescriptor
    // const auto* desc = user_service->GetDescriptor();
    // registry.RegisterService(desc);

    std::cout << "Registered " << registry.Size() << " tools" << std::endl;

    // ========================================================================
    // 步骤 2: 创建 RPC Invoker
    // ========================================================================

    auto invoker = mcp_rpc::MakeKrpcInvoker(false);  // false = 延迟连接

    // ========================================================================
    // 步骤 3: 创建 MCP Session
    // ========================================================================

    mcp_rpc::StreamingConfig config;
    config.aggregate_max_bytes = 1 * 1024 * 1024;  // 1MB
    config.register_client_streaming = false;
    config.register_bidi_streaming = false;

    mcp_rpc::MCPSession session(registry, invoker, config);

    // ========================================================================
    // 步骤 4: 处理 MCP 请求（示例）
    // ========================================================================

    // 模拟 initialize 请求
    std::string init_request = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {}
    })";

    auto init_resp = session.HandleRequest(nlohmann::json::parse(init_request));
    std::cout << "Initialize response: " << init_resp.dump(2) << std::endl;

    // 模拟 tools/list 请求
    std::string list_request = R"({
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/list",
        "params": {}
    })";

    auto list_resp = session.HandleRequest(nlohmann::json::parse(list_request));
    std::cout << "Tools list response: " << list_resp.dump(2) << std::endl;

    std::cout << "\nExample server demo completed." << std::endl;
    std::cout << "To run a real server, implement the UserServiceRpc and start TCP listener." << std::endl;

    return 0;
}
