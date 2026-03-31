/**
 * @file test_integration_e2e.cc
 * @brief MCP-RPC Bridge 端到端集成测试
 *
 * 测试覆盖完整的 MCP-RPC 流程：
 * 1. 服务注册 -> 工具发现 -> JSON-RPC 调用 -> RPC 执行 -> 响应返回
 */

#include "mcp/MCPSession.h"
#include "mcp/ToolRegistry.h"
#include "mcp/StreamingConfig.h"
#include "mcp/StreamingResponseAggregator.h"
#include "invoker/MockRpcInvoker.h"
#include "invoker/KrpcInvoker.h"

#include <iostream>
#include <memory>
#include <cassert>

#include "test_service.pb.h"

using namespace mcp_rpc;

// 辅助函数：打印测试结果
void printTestResult(const std::string& test_name, bool passed) {
    std::cout << (passed ? "[PASS]" : "[FAIL]") << ": " << test_name << std::endl;
}

// 辅助函数：检查 JSON 响应
bool hasError(const nlohmann::json& response, int expected_code = -1) {
    if (!response.contains("error")) return false;
    if (expected_code != -1) {
        return response["error"]["code"] == expected_code;
    }
    return true;
}

bool isSuccess(const nlohmann::json& response) {
    return response.contains("result") && !hasError(response);
}

// 辅助函数：序列化消息
std::vector<uint8_t> SerializeMessage(const google::protobuf::Message& msg) {
    std::vector<uint8_t> bytes(msg.ByteSizeLong());
    msg.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()));
    return bytes;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   MCP-RPC Bridge End-to-End Tests     " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    // 获取服务描述符
    const auto* service_desc = google::protobuf::DescriptorPool::generated_pool()
        ->FindServiceByName("testproto.TestUserServiceRpc");

    if (!service_desc) {
        std::cerr << "ERROR: testproto.TestUserServiceRpc not found!" << std::endl;
        return 1;
    }

    // ============================================
    // 场景 1: 完整的 Unary RPC 流程
    // ============================================
    std::cout << "=== Scenario 1: Unary RPC Complete Flow ===" << std::endl;
    {
        // 1. 创建工具注册表并注册服务
        ToolRegistry registry;
        registry.RegisterService(service_desc);

        // 2. 验证工具已注册
        auto tools = registry.ListTools();
        bool has_list_users = false;
        bool has_get_user = false;
        for (const auto* tool : tools) {
            if (tool->name == "TestUserServiceRpc_ListUsers") has_list_users = true;
            if (tool->name == "TestUserServiceRpc_GetUser") has_get_user = true;
        }
        printTestResult("Service registered with ListUsers and GetUser tools",
                        has_list_users && has_get_user);
        if (has_list_users && has_get_user) passed++; else failed++;

        // 验证工具总数（服务有 6 个方法）
        bool has_six_tools = tools.size() == 6;
        printTestResult("Service has 6 methods registered", has_six_tools);
        if (has_six_tools) passed++; else failed++;

        // 3. 创建 MockInvoker 并配置响应
        auto invoker = std::make_shared<MockRpcInvoker>();

        // 配置 ListUsers 响应
        testproto::ListUsersResponse list_resp;
        list_resp.set_total(2);
        auto* user1 = list_resp.add_users();
        user1->set_id(1);
        user1->set_name("Alice");
        user1->set_email("alice@example.com");
        user1->set_active(true);
        auto* user2 = list_resp.add_users();
        user2->set_id(2);
        user2->set_name("Bob");
        user2->set_email("bob@example.com");
        user2->set_active(false);

        invoker->SetResponse("testproto.TestUserServiceRpc.ListUsers",
                             SerializeMessage(list_resp));

        // 配置 GetUser 响应
        testproto::GetUserResponse get_resp;
        auto* user = get_resp.mutable_user();
        user->set_id(1);
        user->set_name("Alice");
        user->set_email("alice@example.com");
        user->set_active(true);

        invoker->SetResponse("testproto.TestUserServiceRpc.GetUser",
                             SerializeMessage(get_resp));

        // 4. 创建 MCPSession
        MCPSession session(registry, invoker);

        // 5. JSON-RPC: initialize
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        auto init_response = session.HandleRequest(init_request);
        bool init_success = isSuccess(init_response);
        printTestResult("Step 1: initialize succeeds", init_success);
        if (init_success) passed++; else failed++;

        // 6. JSON-RPC: tools/list
        nlohmann::json list_request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/list"},
            {"params", {}}
        };
        auto list_response = session.HandleRequest(list_request);
        bool list_success = isSuccess(list_response);
        printTestResult("Step 2: tools/list succeeds", list_success);
        if (list_success) passed++; else failed++;

        // 验证工具列表
        if (list_success) {
            const auto& tools_result = list_response["result"]["tools"];
            bool has_six_tools = tools_result.is_array() && tools_result.size() == 6;
            printTestResult("Step 2a: tools/list returns 6 tools", has_six_tools);
            if (has_six_tools) passed++; else failed++;
        }

        // 7. JSON-RPC: tools/call - ListUsers
        nlohmann::json call_list_request = {
            {"jsonrpc", "2.0"},
            {"id", 3},
            {"method", "tools/call"},
            {"params", {
                {"name", "TestUserServiceRpc_ListUsers"},
                {"arguments", {{"page", 1}, {"page_size", 10}}}
            }}
        };
        auto call_list_response = session.HandleRequest(call_list_request);
        bool call_list_success = isSuccess(call_list_response);
        printTestResult("Step 3: tools/call ListUsers succeeds", call_list_success);
        if (call_list_success) passed++; else failed++;

        // 验证 ListUsers 响应内容
        if (call_list_success) {
            try {
                const auto& content = call_list_response["result"]["content"];
                const auto& data = content["data"];
                bool has_users = data.contains("users") && data["users"].is_array();
                printTestResult("Step 3a: ListUsers response has users array", has_users);
                if (has_users) passed++; else failed++;

                bool has_total = data.contains("total") && data["total"] == 2;
                printTestResult("Step 3b: ListUsers response has correct total", has_total);
                if (has_total) passed++; else failed++;

                if (has_users) {
                    bool user_names_correct =
                        data["users"][0]["name"] == "Alice" &&
                        data["users"][1]["name"] == "Bob";
                    printTestResult("Step 3c: User names are correct", user_names_correct);
                    if (user_names_correct) passed++; else failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "[FAIL] Step 3: Failed to parse response: " << e.what() << std::endl;
                failed += 3;
            }
        }

        // 8. JSON-RPC: tools/call - GetUser
        nlohmann::json call_get_request = {
            {"jsonrpc", "2.0"},
            {"id", 4},
            {"method", "tools/call"},
            {"params", {
                {"name", "TestUserServiceRpc_GetUser"},
                {"arguments", {{"user_id", 1}}}
            }}
        };
        auto call_get_response = session.HandleRequest(call_get_request);
        bool call_get_success = isSuccess(call_get_response);
        printTestResult("Step 4: tools/call GetUser succeeds", call_get_success);
        if (call_get_success) passed++; else failed++;

        // 验证 GetUser 响应内容
        if (call_get_success) {
            try {
                const auto& content = call_get_response["result"]["content"];
                const auto& data = content["data"];
                bool has_user = data.contains("user");
                printTestResult("Step 4a: GetUser response has user", has_user);
                if (has_user) passed++; else failed++;

                if (has_user) {
                    bool user_correct =
                        data["user"]["id"] == 1 &&
                        data["user"]["name"] == "Alice";
                    printTestResult("Step 4b: User data is correct", user_correct);
                    if (user_correct) passed++; else failed++;
                }
            } catch (const std::exception& e) {
                std::cout << "[FAIL] Step 4: Failed to parse response: " << e.what() << std::endl;
                failed += 2;
            }
        }
    }
    std::cout << std::endl;

    // ============================================
    // 场景 2: Server-Streaming RPC 完整流程
    // ============================================
    std::cout << "=== Scenario 2: Server-Streaming RPC Complete Flow ===" << std::endl;
    {
        ToolRegistry registry;
        registry.RegisterService(service_desc);

        auto invoker = std::make_shared<MockRpcInvoker>();

        // 配置多个流式响应
        testproto::StreamResponse stream1, stream2, stream3;
        stream1.set_data("chunk_1");
        stream2.set_data("chunk_2");
        stream3.set_data("chunk_3");

        // 使用 SetResponse 配置流式响应
        invoker->SetResponse("testproto.TestUserServiceRpc.StreamData",
                             SerializeMessage(stream1));
        invoker->SetResponse("testproto.TestUserServiceRpc.StreamData",
                             SerializeMessage(stream2));
        invoker->SetResponse("testproto.TestUserServiceRpc.StreamData",
                             SerializeMessage(stream3));

        StreamingConfig config;
        config.aggregate_max_bytes = 10 * 1024 * 1024;  // 10MB
        MCPSession session(registry, invoker, config);

        // 1. initialize
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        auto init_response = session.HandleRequest(init_request);
        bool init_success = isSuccess(init_response);
        printTestResult("Streaming: initialize succeeds", init_success);
        if (init_success) passed++; else failed++;

        // 2. tools/call - StreamData
        nlohmann::json stream_request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/call"},
            {"params", {
                {"name", "TestUserServiceRpc_StreamData"},
                {"arguments", {{"count", 3}}}
            }}
        };
        auto stream_response = session.HandleRequest(stream_request);

        bool stream_success = isSuccess(stream_response);
        printTestResult("Streaming: tools/call StreamData succeeds", stream_success);
        if (stream_success) passed++; else failed++;

        if (stream_success) {
            const auto& content = stream_response["result"]["content"];
            bool has_streaming = content.contains("streaming") &&
                                 content["streaming"] == true;
            printTestResult("Streaming: response has streaming=true", has_streaming);
            if (has_streaming) passed++; else failed++;

            bool has_data = content.contains("data");
            printTestResult("Streaming: response has data", has_data);
            if (has_data) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 场景 3: 错误处理 - 参数验证失败
    // ============================================
    std::cout << "=== Scenario 3: Error Handling - Invalid Parameters ===" << std::endl;
    {
        ToolRegistry registry;
        registry.RegisterService(service_desc);

        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        // initialize
        nlohmann::json init_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "initialize"},
            {"params", {}}
        };
        session.HandleRequest(init_request);

        // 调用时缺少必需参数
        nlohmann::json bad_request = {
            {"jsonrpc", "2.0"},
            {"id", 2},
            {"method", "tools/call"},
            {"params", {
                {"name", "TestUserServiceRpc_GetUser"}
                // 缺少 user_id 参数
            }}
        };
        auto response = session.HandleRequest(bad_request);

        // 应该返回错误（参数缺失或 invoker 错误）
        bool has_error_response = hasError(response);
        printTestResult("Missing parameters returns error", has_error_response);
        if (has_error_response) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 场景 4: 状态机 - 未初始化调用失败
    // ============================================
    std::cout << "=== Scenario 4: State Machine - Uninitialized Call ===" << std::endl;
    {
        ToolRegistry registry;
        registry.RegisterService(service_desc);

        auto invoker = std::make_shared<MockRpcInvoker>();
        MCPSession session(registry, invoker);

        // 直接调用 tools/call（未初始化）
        nlohmann::json call_request = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "tools/call"},
            {"params", {
                {"name", "TestUserServiceRpc_GetUser"},
                {"arguments", {{"user_id", 1}}}
            }}
        };
        auto response = session.HandleRequest(call_request);

        bool has_invalid_request_error = hasError(response, -32600);
        printTestResult("Uninitialized call returns InvalidRequest", has_invalid_request_error);
        if (has_invalid_request_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 场景 5: 并发会话（多个独立 Session）
    // ============================================
    std::cout << "=== Scenario 5: Concurrent Sessions ===" << std::endl;
    {
        // 创建多个独立的 session
        std::vector<std::unique_ptr<MCPSession>> sessions;

        for (int i = 0; i < 3; ++i) {
            ToolRegistry registry;
            registry.RegisterService(service_desc);

            auto invoker = std::make_shared<MockRpcInvoker>();

            testproto::GetUserResponse resp;
            auto* user = resp.mutable_user();
            user->set_id(i + 1);
            user->set_name("User" + std::to_string(i + 1));
            user->set_email("user" + std::to_string(i + 1) + "@example.com");

            invoker->SetResponse("testproto.TestUserServiceRpc.GetUser",
                                 SerializeMessage(resp));

            sessions.push_back(std::make_unique<MCPSession>(registry, invoker));
        }

        // 每个 session 独立执行
        bool all_sessions_succeeded = true;
        for (size_t i = 0; i < sessions.size(); ++i) {
            nlohmann::json init_request = {
                {"jsonrpc", "2.0"},
                {"id", 1},
                {"method", "initialize"},
                {"params", {}}
            };
            auto response = sessions[i]->HandleRequest(init_request);
            if (!isSuccess(response)) {
                all_sessions_succeeded = false;
                break;
            }
        }

        printTestResult("All 3 concurrent sessions initialize successfully",
                        all_sessions_succeeded);
        if (all_sessions_succeeded) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 场景 6: StreamingResponseAggregator 直接测试
    // ============================================
    std::cout << "=== Scenario 6: StreamingResponseAggregator Direct Test ===" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 1024 * 1024;
        StreamingResponseAggregator aggregator(config);

        // 创建多个 StreamResponse 消息
        std::vector<std::unique_ptr<google::protobuf::Message>> messages;

        for (int i = 0; i < 5; ++i) {
            auto msg = std::make_unique<testproto::StreamResponse>();
            msg->set_data("stream_chunk_" + std::to_string(i));
            messages.push_back(std::move(msg));
        }

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregator: 5 messages aggregate successfully", success);
        if (success) passed++; else failed++;

        if (success) {
            try {
                auto json = nlohmann::json::parse(result.aggregated_json);
                bool is_array = json.is_array();
                printTestResult("Aggregator: result is JSON array", is_array);
                if (is_array) passed++; else failed++;

                bool has_five_elements = json.size() == 5;
                printTestResult("Aggregator: array has 5 elements", has_five_elements);
                if (has_five_elements) passed++; else failed++;

                // 验证第一个元素
                bool first_correct = json[0]["data"] == "stream_chunk_0";
                printTestResult("Aggregator: first element data correct", first_correct);
                if (first_correct) passed++; else failed++;
            } catch (const std::exception& e) {
                std::cout << "[FAIL] Aggregator: parse error - " << e.what() << std::endl;
                failed += 3;
            }
        }
    }
    std::cout << std::endl;

    // ============================================
    // 总结
    // ============================================
    std::cout << "========================================" << std::endl;
    std::cout << "            Test Summary                " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << std::endl;

    if (failed == 0) {
        std::cout << "*** All E2E integration tests PASSED! ***" << std::endl;
        return 0;
    } else {
        std::cout << "*** Some E2E tests FAILED! ***" << std::endl;
        return 1;
    }
}
