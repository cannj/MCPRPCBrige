/**
 * @file test_mcp_session.cc
 * @brief MCPSession JSON-RPC 协议处理测试
 *
 * 测试覆盖：
 * - JSON-RPC 2.0 完整生命周期：initialize -> tools/list -> tools/call
 * - 状态机转换：New -> Initialized -> Closed
 * - 错误处理：未知方法、未初始化调用、参数错误等
 * - RPC 调用和响应序列化
 */

#include "mcp/MCPSession.h"
#include "mcp/ToolRegistry.h"
#include "invoker/MockRpcInvoker.h"
#include <iostream>
#include <memory>
#include <cassert>

using namespace mcp_rpc;

// 辅助函数：打印测试结果
void printTestResult(const std::string& test_name, bool passed) {
    std::cout << (passed ? "PASS" : "FAIL") << ": " << test_name << std::endl;
}

// 辅助函数：检查 JSON 响应是否包含错误
bool hasError(const nlohmann::json& response, int expected_code = -1) {
    if (!response.contains("error")) {
        return false;
    }
    if (expected_code != -1) {
        return response["error"]["code"] == expected_code;
    }
    return true;
}

// JSON-RPC 错误码常量
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInternalError = -32603;

// 辅助函数：检查 JSON 响应是否成功
bool isSuccess(const nlohmann::json& response) {
    return response.contains("result") && !hasError(response);
}

int main() {
    std::cout << "=== MCPSession Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    // ============================================
    // 测试 1: 初始状态
    // ============================================
    std::cout << "Test 1: Initial state" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        bool is_new = session.GetState() == SessionState::New;
        printTestResult("Initial state is New", is_new);
        if (is_new) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 2: Initialize 请求
    // ============================================
    std::cout << "Test 2: Initialize request" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        // 检查响应结构
        bool is_success = isSuccess(response);
        printTestResult("Response is successful", is_success);
        if (is_success) passed++; else failed++;

        bool has_correct_id = response["id"] == 1;
        printTestResult("Response has correct id", has_correct_id);
        if (has_correct_id) passed++; else failed++;

        // 检查状态转换
        bool state_is_initialized = session.GetState() == SessionState::Initialized;
        printTestResult("State transitions to Initialized", state_is_initialized);
        if (state_is_initialized) passed++; else failed++;

        // 检查响应内容
        if (is_success) {
            const auto& result = response["result"];
            bool has_protocol_version = result.contains("protocolVersion");
            printTestResult("Response has protocolVersion", has_protocol_version);
            if (has_protocol_version) passed++; else failed++;

            bool has_server_info = result.contains("serverInfo");
            printTestResult("Response has serverInfo", has_server_info);
            if (has_server_info) passed++; else failed++;

            bool has_capabilities = result.contains("capabilities");
            printTestResult("Response has capabilities", has_capabilities);
            if (has_capabilities) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 3: tools/list 请求（空注册表）
    // ============================================
    std::cout << "Test 3: tools/list request (empty registry)" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        // 先初始化
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        session.HandleRequest(init_request);

        // 请求 tools/list
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/list"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        bool is_success = isSuccess(response);
        printTestResult("Response is successful", is_success);
        if (is_success) passed++; else failed++;

        bool has_tools = response["result"].contains("tools");
        printTestResult("Response has tools array", has_tools);
        if (has_tools) passed++; else failed++;

        bool tools_is_empty = response["result"]["tools"].is_array() &&
                              response["result"]["tools"].empty();
        printTestResult("Tools array is empty", tools_is_empty);
        if (tools_is_empty) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 4: 未初始化时调用 tools/list 应该失败
    // ============================================
    std::cout << "Test 4: tools/list before init fails" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "tools/list"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        bool has_expected_error = hasError(response, kInvalidRequest);
        printTestResult("Returns InvalidRequest error", has_expected_error);
        if (has_expected_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 5: 未初始化时调用 tools/call 应该失败
    // ============================================
    std::cout << "Test 5: tools/call before init fails" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "tools/call"},
            {"params", {{"name", "test"}, {"arguments", {}}}}
        };

        auto response = session.HandleRequest(request);

        bool has_expected_error = hasError(response, kInvalidRequest);
        printTestResult("Returns InvalidRequest error", has_expected_error);
        if (has_expected_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 6: 未知方法应该返回 MethodNotFound
    // ============================================
    std::cout << "Test 6: Unknown method returns error" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        // 先初始化
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        session.HandleRequest(init_request);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "unknown/method"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        bool has_expected_error = hasError(response, kMethodNotFound);
        printTestResult("Returns MethodNotFound error", has_expected_error);
        if (has_expected_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 7: tools/call 调用未知工具
    // ============================================
    std::cout << "Test 7: tools/call unknown tool" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        // 先初始化
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        session.HandleRequest(init_request);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/call"},
            {"params", {{"name", "NonExistentTool"}, {"arguments", {}}}}
        };

        auto response = session.HandleRequest(request);

        bool has_expected_error = hasError(response, kMethodNotFound);
        printTestResult("Returns MethodNotFound error", has_expected_error);
        if (has_expected_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 8: tools/call 成功调用（使用 MockRpcInvoker）
    // ============================================
    std::cout << "Test 8: tools/call with mock response" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();

        // 配置默认响应
        std::vector<uint8_t> mock_response = {0x08, 0x01, 0x12, 0x04, 0x74, 0x65, 0x73, 0x74};
        invoker->SetDefaultResponse(mock_response);

        MCPSession session(registry, invoker);

        // 先初始化
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        session.HandleRequest(init_request);

        // 注意：由于 registry 为空，这个测试会返回 MethodNotFound
        // 完整的工具调用测试需要注册工具
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/call"},
            {"params", {{"name", "TestTool"}, {"arguments", {}}}}
        };

        auto response = session.HandleRequest(request);

        // 由于没有注册工具，应该返回 MethodNotFound
        bool has_expected_error = hasError(response, kMethodNotFound);
        printTestResult("Returns MethodNotFound for unregistered tool", has_expected_error);
        if (has_expected_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 9: JSON-RPC 请求缺少 id 字段（notification）
    // ============================================
    std::cout << "Test 9: Notification request (no id)" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"method", "initialize"},
            {"params", {}}
        };

        auto response = session.HandleRequest(request);

        // 对于 notification，响应中的 id 应该是 null 或者不存在
        // JSON-RPC 2.0 规定：如果请求中没有 id，响应也应该没有 id
        bool is_valid = !response.contains("id") || response["id"].is_null();
        printTestResult("Notification has no id or null id", is_valid);
        if (is_valid) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 10: 非法的 JSON-RPC 请求
    // ============================================
    std::cout << "Test 10: Invalid JSON-RPC request" << std::endl;
    {
        ToolRegistry registry;
        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        nlohmann::json request = {
            {"jsonrpc", "1.0"},  // 错误版本
            {"id", 1},
            {"method", "initialize"}
        };

        auto response = session.HandleRequest(request);

        // 应该能处理，但可能返回错误
        bool has_jsonrpc = response.contains("jsonrpc");
        printTestResult("Response has jsonrpc field", has_jsonrpc);
        if (has_jsonrpc) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 总结
    // ============================================
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << std::endl;

    if (failed == 0) {
        std::cout << "All MCPSession tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
