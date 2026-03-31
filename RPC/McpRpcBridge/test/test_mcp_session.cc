/**
 * @file test_mcp_session.cc
 * @brief MCPSession JSON-RPC 协议处理测试
 */

#include "mcp/MCPSession.h"
#include "mcp/ToolRegistry.h"
#include "invoker/RpcInvoker.h"
#include <iostream>
#include <memory>

using namespace mcp_rpc;

// Mock RpcInvoker 用于测试
class MockRpcInvoker : public RpcInvoker {
public:
    std::future<StatusOr<std::vector<uint8_t>>> Invoke(
        std::string_view method_full_name,
        std::span<const uint8_t> request_bytes) override {

        // 返回空的响应
        std::promise<StatusOr<std::vector<uint8_t>>> promise;
        promise.set_value(std::vector<uint8_t>{});
        return promise.get_future();
    }
};

int main() {
    std::cout << "Testing MCPSession..." << std::endl;

    // 创建依赖
    ToolRegistry registry;
    auto invoker = std::make_shared<MockRpcInvoker>();
    StreamingConfig config;

    MCPSession session(registry, invoker, config);

    // 测试 1: 初始状态
    if (session.GetState() != SessionState::New) {
        std::cerr << "FAIL: Initial state should be New" << std::endl;
        return 1;
    }
    std::cout << "PASS: Initial state is New" << std::endl;

    // 测试 2: Initialize 请求
    {
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        if (response["jsonrpc"] != "2.0" ||
            response["id"] != 1 ||
            response.contains("error")) {
            std::cerr << "FAIL: Initialize response invalid: " << response.dump() << std::endl;
            return 1;
        }

        if (session.GetState() != SessionState::Initialized) {
            std::cerr << "FAIL: State should be Initialized after initialize" << std::endl;
            return 1;
        }

        std::cout << "PASS: Initialize request" << std::endl;
    }

    // 测试 3: tools/list 请求
    {
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/list"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        if (!response.contains("result") ||
            !response["result"].contains("tools")) {
            std::cerr << "FAIL: tools/list response invalid: " << response.dump() << std::endl;
            return 1;
        }

        std::cout << "PASS: tools/list request" << std::endl;
    }

    // 测试 4: 未初始化时调用 tools/call 应该失败
    {
        MCPSession session2(registry, invoker, config);  // 新会话，未初始化

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 3},
            {"method", "tools/call"},
            {"params", {{"name", "test"}, {"arguments", {}}}}
        };

        auto response = session2.HandleRequest(request);

        if (!response.contains("error") ||
            response["error"]["code"] != -32600) {
            std::cerr << "FAIL: tools/call before init should fail: " << response.dump() << std::endl;
            return 1;
        }

        std::cout << "PASS: tools/call before init fails correctly" << std::endl;
    }

    // 测试 5: 未知方法应该返回错误
    {
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 4},
            {"method", "unknown/method"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        if (!response.contains("error") ||
            response["error"]["code"] != -32601) {
            std::cerr << "FAIL: Unknown method should return MethodNotFound: " << response.dump() << std::endl;
            return 1;
        }

        std::cout << "PASS: Unknown method returns error" << std::endl;
    }

    std::cout << "\nAll MCPSession tests passed!" << std::endl;
    return 0;
}
