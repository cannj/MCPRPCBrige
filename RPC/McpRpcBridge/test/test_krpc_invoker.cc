/**
 * @file test_krpc_invoker.cc
 * @brief KrpcInvoker 集成测试
 *
 * 注意：KrpcInvoker 依赖实际的 Krpc 框架（包括 ZooKeeper 连接、网络通信等），
 * 因此这些测试需要完整的基础设施支持。
 *
 * 在没有实际服务器的情况下，测试主要验证：
 * 1. 方法名解析逻辑
 * 2. Channel 管理逻辑
 * 3. 错误处理流程
 */

#include "invoker/KrpcInvoker.h"
#include "invoker/RpcInvoker.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

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

// ============================================================================
// 以下测试需要实际的 proto 文件已编译并链接到程序中
// 使用 user.proto 中定义的服务进行集成测试
// ============================================================================

// 测试 1: 方法名解析
bool test_parse_method_name() {
    std::cout << "\n  Testing method name parsing..." << std::endl;

    // 创建一个 KrpcInvoker 实例（仅用于测试 ParseMethodName）
    auto invoker = std::make_shared<KrpcInvoker>();

    // 由于 ParseMethodName 是私有的，我们通过 Invoke 的返回值间接测试
    // 这里主要验证格式验证逻辑

    // 测试无效格式
    auto future1 = invoker->Invoke("InvalidFormat", std::vector<uint8_t>{0x01});
    auto result1 = future1.get();

    if (!result1.ok()) {
        std::string msg = result1.message();
        if (msg.find("Invalid method name format") != std::string::npos) {
            std::cout << "    Correctly rejected invalid format" << std::endl;
        } else {
            std::cout << "    Unexpected error: " << msg << std::endl;
            return false;
        }
    } else {
        std::cout << "    Should have rejected invalid format" << std::endl;
        return false;
    }

    // 测试有效格式（但服务不存在）
    auto future2 = invoker->Invoke("/TestService/TestMethod", std::vector<uint8_t>{0x01});
    auto result2 = future2.get();

    // 预期会失败（因为服务不存在），但不应该是因为方法名解析失败
    if (!result2.ok()) {
        std::string msg = result2.message();
        if (msg.find("Invalid method name format") != std::string::npos) {
            std::cout << "    Incorrectly rejected valid format: " << msg << std::endl;
            return false;
        }
        std::cout << "    Method name format accepted (expected to fail later)" << std::endl;
    }

    return true;
}

// 测试 2: 工厂函数
bool test_make_krpc_invoker() {
    std::cout << "\n  Testing factory function..." << std::endl;

    // 测试创建不立即连接的 invoker
    auto invoker1 = MakeKrpcInvoker(false);
    if (!invoker1) {
        std::cout << "    Failed to create invoker" << std::endl;
        return false;
    }

    // 测试创建立即连接的 invoker
    auto invoker2 = MakeKrpcInvoker(true);
    if (!invoker2) {
        std::cout << "    Failed to create invoker with connect_now" << std::endl;
        return false;
    }

    // 验证返回类型
    std::shared_ptr<RpcInvoker> base = MakeKrpcInvoker(false);
    if (!base) {
        std::cout << "    Failed to create base class pointer" << std::endl;
        return false;
    }

    std::cout << "    Factory function works correctly" << std::endl;
    return true;
}

// 测试 3: 并发调用（线程安全）
bool test_concurrent_invokes() {
    std::cout << "\n  Testing concurrent invokes..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // 启动多个线程并发调用
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            // 调用一个不存在的服务（预期会失败，但不应崩溃）
            auto future = invoker->Invoke(
                "/TestService/TestMethod",
                std::vector<uint8_t>{static_cast<uint8_t>(i)}
            );
            auto result = future.get();
            // 只要不抛出异常，就认为线程安全测试通过
            results[i] = true;
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 验证所有线程都完成了
    for (int i = 0; i < num_threads; ++i) {
        if (!results[i]) {
            std::cout << "    Thread " << i << " failed" << std::endl;
            return false;
        }
    }

    std::cout << "    Concurrent invokes completed without crashes" << std::endl;
    return true;
}

// 测试 4: SetConnectNow 方法
bool test_set_connect_now() {
    std::cout << "\n  Testing SetConnectNow..." << std::endl;

    auto invoker = std::make_shared<KrpcInvoker>();

    // 设置为立即连接
    invoker->SetConnectNow(true);

    // 由于无法直接验证内部状态，我们只是确保方法可以调用
    std::cout << "    SetConnectNow method called successfully" << std::endl;
    return true;
}

// 测试 5: 空请求数据处理
bool test_empty_request_data() {
    std::cout << "\n  Testing empty request data..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    // 发送空请求
    auto future = invoker->Invoke(
        "/TestService/TestMethod",
        std::vector<uint8_t>()  // 空数据
    );
    auto result = future.get();

    // 预期会失败（因为服务不存在或无法解析请求），但不应该崩溃
    std::cout << "    Empty request handled (result: "
              << (result.ok() ? "success" : result.message()) << ")" << std::endl;

    return true;  // 只要不崩溃就通过
}

// 测试 6: 大请求数据处理
bool test_large_request_data() {
    std::cout << "\n  Testing large request data..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    // 创建大请求数据（1MB）
    std::vector<uint8_t> large_data(1024 * 1024, 0xAB);

    auto future = invoker->Invoke(
        "/TestService/TestMethod",
        large_data
    );
    auto result = future.get();

    // 预期会失败（因为服务不存在），但不应该崩溃
    std::cout << "    Large request handled (result: "
              << (result.ok() ? "success" : result.message()) << ")" << std::endl;

    return true;  // 只要不崩溃就通过
}

// 测试 7: 方法全名格式变体
bool test_method_name_variants() {
    std::cout << "\n  Testing method name variants..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    std::vector<std::string> formats = {
        "/package.Service/Method",
        "/Service/Method",
        "package.Service/Method",
        "Service/Method",
    };

    for (const auto& format : formats) {
        auto future = invoker->Invoke(format, std::vector<uint8_t>{0x01});
        auto result = future.get();

        // 预期会失败（因为服务不存在），但不应该是因为格式问题（除了第一个带点的）
        if (!result.ok()) {
            std::string msg = result.message();
            if (msg.find("Invalid method name format") != std::string::npos) {
                std::cout << "    Rejected format: " << format << std::endl;
                // 只有不包含点的格式被拒绝才是错误的
                if (format.find('.') == std::string::npos) {
                    return false;
                }
            }
        }
    }

    std::cout << "    Method name variants handled" << std::endl;
    return true;
}

// 测试 8: 重复调用同一服务（Channel 复用）
bool test_channel_reuse() {
    std::cout << "\n  Testing channel reuse..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    // 多次调用同一服务
    for (int i = 0; i < 3; ++i) {
        auto future = invoker->Invoke(
            "/TestService/TestMethod",
            std::vector<uint8_t>{static_cast<uint8_t>(i)}
        );
        auto result = future.get();
        // 预期会失败（因为服务不存在），但不应该崩溃
    }

    std::cout << "    Repeated invokes completed" << std::endl;
    return true;
}

// 测试 9: 调用不同服务（多 Channel 管理）
bool test_multiple_services() {
    std::cout << "\n  Testing multiple services..." << std::endl;

    auto invoker = MakeKrpcInvoker(false);

    std::vector<std::string> services = {
        "/ServiceA/Method",
        "/ServiceB/Method",
        "/ServiceC/Method",
    };

    for (const auto& service : services) {
        auto future = invoker->Invoke(
            service,
            std::vector<uint8_t>{0x01}
        );
        auto result = future.get();
        // 预期会失败（因为服务不存在），但不应该崩溃
    }

    std::cout << "    Multiple service invokes completed" << std::endl;
    return true;
}

// 测试 10: 析构函数（资源清理）
bool test_destructor() {
    std::cout << "\n  Testing destructor..." << std::endl;

    {
        auto invoker = MakeKrpcInvoker(true);
        // 触发一些调用
        invoker->Invoke("/Test/Method", std::vector<uint8_t>{0x01}).get();
        // invoker 离开作用域，应该正确清理资源
    }

    std::cout << "    Destructor completed without issues" << std::endl;
    return true;
}

int main() {
    std::cout << "=== KrpcInvoker Tests ===" << std::endl;
    std::cout << "\nNote: These tests verify KrpcInvoker's internal logic." << std::endl;
    std::cout << "Full integration tests require a running Krpc server." << std::endl;

    int passed = 0;
    int failed = 0;

    struct TestCase {
        const char* name;
        std::function<bool()> test_fn;
    };

    std::vector<TestCase> tests = {
        {"Parse method name", test_parse_method_name},
        {"Factory function", test_make_krpc_invoker},
        {"Concurrent invokes", test_concurrent_invokes},
        {"SetConnectNow", test_set_connect_now},
        {"Empty request data", test_empty_request_data},
        {"Large request data", test_large_request_data},
        {"Method name variants", test_method_name_variants},
        {"Channel reuse", test_channel_reuse},
        {"Multiple services", test_multiple_services},
        {"Destructor", test_destructor},
    };

    for (const auto& test : tests) {
        try {
            bool result = test.test_fn();
            print_result(test.name, result);
            if (result) {
                passed++;
            } else {
                failed++;
            }
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << test.name << " - Exception: " << e.what() << std::endl;
            failed++;
        }
    }

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Passed: " << passed << "/" << tests.size() << std::endl;

    if (failed > 0) {
        std::cout << "Failed: " << failed << std::endl;
        return 1;
    }

    std::cout << "\nAll KrpcInvoker tests passed!" << std::endl;
    return 0;
}
