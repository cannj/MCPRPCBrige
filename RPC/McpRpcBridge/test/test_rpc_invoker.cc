/**
 * @file test_rpc_invoker.cc
 * @brief RpcInvoker 接口及 Mock 实现端到端测试
 */

#include "invoker/MockRpcInvoker.h"
#include "invoker/RpcInvoker.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>

using namespace mcp_rpc;

// 辅助函数：打印测试结果
void print_result(const std::string& test_name, bool passed) {
    std::cout << (passed ? "PASS" : "FAIL") << ": " << test_name << std::endl;
}

// 辅助函数：字节向量比较
bool bytes_equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// 测试 1: 基本调用 - 成功响应
bool test_basic_success_response() {
    auto invoker = MakeMockRpcInvoker();

    // 配置响应
    std::vector<uint8_t> expected_response = {0x01, 0x02, 0x03, 0x04};
    invoker->SetResponse("/test.Service/TestMethod", expected_response);

    // 发起调用
    std::vector<uint8_t> request = {0x0A, 0x0B};
    auto future = invoker->Invoke("/test.Service/TestMethod", request);

    // 等待结果
    auto result = future.get();

    // 验证
    if (!result.ok()) {
        std::cerr << "  Expected success, got error: " << result.message() << std::endl;
        return false;
    }

    const auto& response = result.value();
    if (!bytes_equal(response, expected_response)) {
        std::cerr << "  Response mismatch" << std::endl;
        return false;
    }

    return true;
}

// 测试 2: 基本调用 - 错误响应
bool test_basic_error_response() {
    auto invoker = MakeMockRpcInvoker();

    // 配置错误响应
    invoker->SetErrorResponse("/test.Service/TestMethod",
                              StatusCode::ERROR,
                              "Method not found");

    // 发起调用
    std::vector<uint8_t> request = {0x0A, 0x0B};
    auto future = invoker->Invoke("/test.Service/TestMethod", request);

    // 等待结果
    auto result = future.get();

    // 验证
    if (result.ok()) {
        std::cerr << "  Expected error, got success" << std::endl;
        return false;
    }

    if (result.message() != "Method not found") {
        std::cerr << "  Error message mismatch: " << result.message() << std::endl;
        return false;
    }

    return true;
}

// 测试 3: 默认响应配置
bool test_default_response() {
    auto invoker = MakeMockRpcInvoker();

    // 配置默认响应
    std::vector<uint8_t> default_response = {0xFF, 0xFF};
    invoker->SetDefaultResponse(default_response);

    // 调用未配置的方法
    std::vector<uint8_t> request = {0x01};
    auto future = invoker->Invoke("/unconfigured.Service/Method", request);
    auto result = future.get();

    // 验证
    if (!result.ok()) {
        std::cerr << "  Expected success with default response" << std::endl;
        return false;
    }

    if (!bytes_equal(result.value(), default_response)) {
        std::cerr << "  Default response mismatch" << std::endl;
        return false;
    }

    return true;
}

// 测试 4: 方法级配置优先于默认配置
bool test_method_override_default() {
    auto invoker = MakeMockRpcInvoker();

    // 配置默认响应
    invoker->SetDefaultResponse({0xAA, 0xAA});

    // 配置特定方法响应
    std::vector<uint8_t> specific_response = {0xBB, 0xBB};
    invoker->SetResponse("/test.Service/SpecificMethod", specific_response);

    // 调用特定方法
    auto future1 = invoker->Invoke("/test.Service/SpecificMethod", std::vector<uint8_t>{0x01});
    auto result1 = future1.get();

    // 调用其他方法（应使用默认响应）
    auto future2 = invoker->Invoke("/test.Service/OtherMethod", std::vector<uint8_t>{0x02});
    auto result2 = future2.get();

    // 验证
    if (!result1.ok() || !result2.ok()) {
        std::cerr << "  Expected success for both calls" << std::endl;
        return false;
    }

    if (!bytes_equal(result1.value(), specific_response)) {
        std::cerr << "  Specific method response mismatch" << std::endl;
        return false;
    }

    if (!bytes_equal(result2.value(), {0xAA, 0xAA})) {
        std::cerr << "  Default response not used" << std::endl;
        return false;
    }

    return true;
}

// 测试 5: 调用历史记录
bool test_call_history() {
    auto invoker = MakeMockRpcInvoker();

    invoker->SetResponse("/test.Service/Method1", {0x01});
    invoker->SetResponse("/test.Service/Method2", {0x02});

    // 多次调用
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0xA1}).get();
    invoker->Invoke("/test.Service/Method2", std::vector<uint8_t>{0xA2}).get();
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0xA3}).get();

    // 验证历史记录
    auto history = invoker->GetCallHistory();

    if (history.size() != 3) {
        std::cerr << "  Expected 3 history entries, got " << history.size() << std::endl;
        return false;
    }

    if (history[0].first != "/test.Service/Method1" ||
        history[1].first != "/test.Service/Method2" ||
        history[2].first != "/test.Service/Method1") {
        std::cerr << "  History method names incorrect" << std::endl;
        return false;
    }

    // 验证请求数据
    if (history[0].second[0] != 0xA1 ||
        history[1].second[0] != 0xA2 ||
        history[2].second[0] != 0xA3) {
        std::cerr << "  History request data incorrect" << std::endl;
        return false;
    }

    return true;
}

// 测试 6: 调用计数
bool test_call_count() {
    auto invoker = MakeMockRpcInvoker();

    invoker->SetResponse("/test.Service/Method1", {0x01});
    invoker->SetResponse("/test.Service/Method2", {0x02});

    // 调用
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0x01}).get();
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0x02}).get();
    invoker->Invoke("/test.Service/Method2", std::vector<uint8_t>{0x03}).get();

    // 验证计数
    if (invoker->GetCallCount("/test.Service/Method1") != 2) {
        std::cerr << "  Method1 call count should be 2" << std::endl;
        return false;
    }

    if (invoker->GetCallCount("/test.Service/Method2") != 1) {
        std::cerr << "  Method2 call count should be 1" << std::endl;
        return false;
    }

    if (invoker->GetTotalCallCount() != 3) {
        std::cerr << "  Total call count should be 3" << std::endl;
        return false;
    }

    return true;
}

// 测试 7: 清除历史
bool test_clear_history() {
    auto invoker = MakeMockRpcInvoker();

    invoker->SetResponse("/test.Service/Method1", {0x01});

    // 调用
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0x01}).get();
    invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0x02}).get();

    // 清除历史
    invoker->ClearHistory();

    // 验证
    if (invoker->GetTotalCallCount() != 0) {
        std::cerr << "  Total call count should be 0 after clear" << std::endl;
        return false;
    }

    if (!invoker->GetCallHistory().empty()) {
        std::cerr << "  History should be empty after clear" << std::endl;
        return false;
    }

    return true;
}

// 测试 8: 清除配置
bool test_clear_config() {
    auto invoker = MakeMockRpcInvoker();

    invoker->SetResponse("/test.Service/Method1", {0x01});
    invoker->ClearConfig();

    // 配置清除后调用应该失败
    auto future = invoker->Invoke("/test.Service/Method1", std::vector<uint8_t>{0x01});
    auto result = future.get();

    if (result.ok()) {
        std::cerr << "  Should fail after config cleared" << std::endl;
        return false;
    }

    return true;
}

// 测试 9: 模拟延迟
bool test_simulated_delay() {
    auto invoker = MakeMockRpcInvoker();
    invoker->SetResponse("/test.Service/Method", {0x01});
    invoker->SetSimulatedDelay(100);  // 100ms 延迟

    auto start = std::chrono::steady_clock::now();
    invoker->Invoke("/test.Service/Method", std::vector<uint8_t>{0x01}).get();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (duration.count() < 100) {
        std::cerr << "  Expected at least 100ms delay, got " << duration.count() << "ms" << std::endl;
        return false;
    }

    return true;
}

// 测试 10: 并发调用（线程安全）
bool test_concurrent_calls() {
    auto invoker = MakeMockRpcInvoker();
    invoker->SetResponse("/test.Service/Method", {0x01});

    const int num_threads = 10;
    const int calls_per_thread = 5;
    std::vector<std::thread> threads;

    // 启动多个线程并发调用
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&invoker, calls_per_thread]() {
            for (int j = 0; j < calls_per_thread; ++j) {
                auto future = invoker->Invoke("/test.Service/Method",
                                              std::vector<uint8_t>{static_cast<uint8_t>(j)});
                auto result = future.get();
                if (!result.ok()) {
                    std::cerr << "  Concurrent call failed" << std::endl;
                    return;
                }
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 验证总调用次数
    int expected_calls = num_threads * calls_per_thread;
    if (invoker->GetTotalCallCount() != expected_calls) {
        std::cerr << "  Expected " << expected_calls << " calls, got "
                  << invoker->GetTotalCallCount() << std::endl;
        return false;
    }

    return true;
}

// 测试 11: 未配置方法的默认错误
bool test_unconfigured_default_error() {
    auto invoker = MakeMockRpcInvoker();

    // 不配置任何响应，也不配置默认响应
    // 应该返回 "No response configured" 错误

    auto future = invoker->Invoke("/test.Service/Method", std::vector<uint8_t>{0x01});
    auto result = future.get();

    if (result.ok()) {
        std::cerr << "  Should fail for unconfigured method" << std::endl;
        return false;
    }

    // 检查错误消息是否包含预期内容
    const std::string& msg = result.message();
    if (msg.find("No response configured") == std::string::npos) {
        std::cerr << "  Unexpected error message: " << msg << std::endl;
        return false;
    }

    return true;
}

// 测试 12: 默认错误响应
bool test_default_error_response() {
    auto invoker = MakeMockRpcInvoker();

    // 配置默认错误响应
    invoker->SetDefaultErrorResponse(StatusCode::ERROR, "Default error message");

    // 调用未配置的方法
    auto future = invoker->Invoke("/unconfigured.Service/Method",
                                   std::vector<uint8_t>{0x01});
    auto result = future.get();

    if (result.ok()) {
        std::cerr << "  Should return configured default error" << std::endl;
        return false;
    }

    if (result.message() != "Default error message") {
        std::cerr << "  Error message mismatch: " << result.message() << std::endl;
        return false;
    }

    return true;
}

int main() {
    std::cout << "=== RpcInvoker Mock End-to-End Tests ===" << std::endl << std::endl;

    int passed = 0;
    int failed = 0;

    struct TestCase {
        const char* name;
        std::function<bool()> test_fn;
    };

    std::vector<TestCase> tests = {
        {"Basic success response", test_basic_success_response},
        {"Basic error response", test_basic_error_response},
        {"Default response", test_default_response},
        {"Method override default", test_method_override_default},
        {"Call history", test_call_history},
        {"Call count", test_call_count},
        {"Clear history", test_clear_history},
        {"Clear config", test_clear_config},
        {"Simulated delay", test_simulated_delay},
        {"Concurrent calls (thread safety)", test_concurrent_calls},
        {"Unconfigured default error", test_unconfigured_default_error},
        {"Default error response", test_default_error_response},
    };

    for (const auto& test : tests) {
        bool result = test.test_fn();
        print_result(test.name, result);
        if (result) {
            passed++;
        } else {
            failed++;
        }
    }

    std::cout << std::endl;
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << tests.size() << std::endl;

    if (failed > 0) {
        std::cout << "Failed: " << failed << std::endl;
        return 1;
    }

    std::cout << "\nAll RpcInvoker tests passed!" << std::endl;
    return 0;
}
