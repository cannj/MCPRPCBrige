/**
 * @file test_lazy_value_complete.cc
 * @brief LazyValue 完整单元测试
 *
 * 测试覆盖：
 * 1. 基本延迟初始化
 * 2. 缓存命中（初始化函数只调用一次）
 * 3. 并发安全（多线程同时初始化）
 * 4. Reset 功能
 * 5. 复杂类型支持
 * 6. 异常安全
 */

#include "mcp/LazyValue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>

using namespace mcp_rpc;

// 测试计数器
struct TestStats {
    int passed = 0;
    int failed = 0;
};

#define TEST_CASE(name) std::cout << "Running: " << name << "... ";
#define PASS() do { stats.passed++; std::cout << "PASS" << std::endl; } while(0)
#define FAIL(msg) do { stats.failed++; std::cerr << "FAIL: " << msg << std::endl; } while(0)

// 测试 1: 基本延迟初始化
void test_basic_initialization(TestStats& stats) {
    TEST_CASE("Basic lazy initialization");

    LazyValue<int> lazy;

    // 初始状态应该是未初始化
    if (lazy.IsInitialized()) {
        FAIL("LazyValue should not be initialized initially");
        return;
    }

    // 获取值，触发初始化
    int value = lazy.GetOrInit([]() { return 42; });

    if (value != 42) {
        FAIL("Expected 42, got " + std::to_string(value));
        return;
    }

    // 现在应该是已初始化状态
    if (!lazy.IsInitialized()) {
        FAIL("LazyValue should be initialized after GetOrInit");
        return;
    }

    PASS();
}

// 测试 2: 缓存命中
void test_cache_hit(TestStats& stats) {
    TEST_CASE("Cache hit (init called once)");

    std::atomic<int> call_count{0};
    LazyValue<int> lazy;

    int v1 = lazy.GetOrInit([&call_count]() {
        call_count++;
        return 100;
    });

    int v2 = lazy.GetOrInit([&call_count]() {
        call_count++;  // 不应该被调用
        return 200;
    });

    int v3 = lazy.GetOrInit([&call_count]() {
        call_count++;  // 不应该被调用
        return 300;
    });

    if (v1 != 100 || v2 != 100 || v3 != 100) {
        FAIL("Cached value mismatch");
        return;
    }

    if (call_count != 1) {
        FAIL("Init function called " + std::to_string(call_count) + " times, expected 1");
        return;
    }

    PASS();
}

// 测试 3: 并发安全
void test_thread_safety(TestStats& stats) {
    TEST_CASE("Thread-safe initialization");

    LazyValue<int> lazy;
    std::atomic<int> call_count{0};
    std::atomic<int> success_count{0};

    const int num_threads = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&lazy, &call_count, &success_count, i]() {
            int value = lazy.GetOrInit([&call_count]() {
                call_count++;
                // 模拟一些工作负载
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                return 999;
            });
            if (value == 999) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (call_count != 1) {
        FAIL("Init called " + std::to_string(call_count) + " times, expected 1");
        return;
    }

    if (success_count != num_threads) {
        FAIL("Not all threads got the correct value");
        return;
    }

    PASS();
}

// 测试 4: Reset 功能
void test_reset(TestStats& stats) {
    TEST_CASE("Reset functionality");

    std::atomic<int> call_count{0};
    LazyValue<int> lazy;

    // 第一次初始化
    int v1 = lazy.GetOrInit([&call_count]() {
        call_count++;
        return 100;
    });

    if (v1 != 100 || call_count != 1) {
        FAIL("First initialization failed");
        return;
    }

    // 重置
    lazy.Reset();

    if (lazy.IsInitialized()) {
        FAIL("Should be uninitialized after Reset");
        return;
    }

    // 第二次初始化
    int v2 = lazy.GetOrInit([&call_count]() {
        call_count++;
        return 200;
    });

    if (v2 != 200 || call_count != 2) {
        FAIL("Second initialization failed");
        return;
    }

    PASS();
}

// 测试 5: 复杂类型支持
void test_complex_type(TestStats& stats) {
    TEST_CASE("Complex type support");

    struct ComplexData {
        std::string name;
        int value;
        std::vector<int> items;

        ComplexData() : name("default"), value(0) {}
        ComplexData(const std::string& n, int v) : name(n), value(v) {
            for (int i = 0; i < v; ++i) {
                items.push_back(i);
            }
        }
    };

    LazyValue<ComplexData> lazy;

    auto& data = lazy.GetOrInit([]() {
        return ComplexData("test", 5);
    });

    if (data.name != "test" || data.value != 5 || data.items.size() != 5) {
        FAIL("Complex data initialization failed");
        return;
    }

    PASS();
}

// 测试 6: 异常安全
void test_exception_safety(TestStats& stats) {
    TEST_CASE("Exception safety");

    LazyValue<int> lazy;
    bool exception_thrown = false;

    try {
        lazy.GetOrInit([]() -> int {
            throw std::runtime_error("Test exception");
        });
    } catch (const std::runtime_error& e) {
        exception_thrown = true;
    }

    if (!exception_thrown) {
        FAIL("Should have thrown exception");
        return;
    }

    // 异常后应该仍然可以初始化
    int value = lazy.GetOrInit([]() { return 42; });
    if (value != 42) {
        FAIL("Recovery after exception failed");
        return;
    }

    PASS();
}

// 测试 7: 返回值是引用
void test_reference_return(TestStats& stats) {
    TEST_CASE("Reference return");

    LazyValue<std::string> lazy;

    const std::string& ref1 = lazy.GetOrInit([]() {
        return std::string("hello");
    });

    const std::string& ref2 = lazy.GetOrInit([]() {
        return std::string("world");  // 不应该被调用
    });

    // 两次调用应该返回同一个引用
    if (&ref1 != &ref2) {
        FAIL("References should point to same object");
        return;
    }

    if (ref1 != "hello") {
        FAIL("Value should be 'hello'");
        return;
    }

    PASS();
}

// 测试 8: 移动语义
void test_move_semantics(TestStats& stats) {
    TEST_CASE("Move semantics");

    LazyValue<int> lazy1;
    lazy1.GetOrInit([]() { return 42; });

    // 移动构造
    LazyValue<int> lazy2 = std::move(lazy1);

    if (lazy1.IsInitialized()) {
        FAIL("Source should be uninitialized after move");
        return;
    }

    if (!lazy2.IsInitialized()) {
        FAIL("Destination should be initialized after move");
        return;
    }

    if (*lazy2.Peek() != 42) {
        FAIL("Value mismatch after move");
        return;
    }

    // 移动赋值
    LazyValue<int> lazy3;
    lazy3 = std::move(lazy2);

    if (lazy2.IsInitialized()) {
        FAIL("Source should be uninitialized after move assignment");
        return;
    }

    if (!lazy3.IsInitialized()) {
        FAIL("Destination should be initialized after move assignment");
        return;
    }

    PASS();
}

// 测试 9: Peek 方法
void test_peek(TestStats& stats) {
    TEST_CASE("Peek method");

    LazyValue<int> lazy;

    // 未初始化时 Peek 返回 nullptr
    if (lazy.Peek() != nullptr) {
        FAIL("Peek should return nullptr when uninitialized");
        return;
    }

    lazy.GetOrInit([]() { return 42; });

    const int* val = lazy.Peek();
    if (val == nullptr) {
        FAIL("Peek should return value when initialized");
        return;
    }

    if (*val != 42) {
        FAIL("Peeked value mismatch");
        return;
    }

    lazy.Reset();
    if (lazy.Peek() != nullptr) {
        FAIL("Peek should return nullptr after Reset");
        return;
    }

    PASS();
}

// 测试 10: Reset 与 GetOrInit 的并发安全
void test_reset_concurrency(TestStats& stats) {
    TEST_CASE("Reset vs GetOrInit concurrency");

    LazyValue<int> lazy;
    std::atomic<int> init_count{0};
    std::atomic<bool> stop{false};
    std::atomic<int> exceptions{0};

    // Reset 线程
    std::thread resetter([&lazy, &stop]() {
        while (!stop.load()) {
            lazy.Reset();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // 初始化线程
    std::thread initializer([&lazy, &init_count, &exceptions]() {
        for (int i = 0; i < 50; ++i) {
            try {
                lazy.GetOrInit([&init_count]() {
                    init_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    return 42;
                });
            } catch (...) {
                exceptions++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);
    resetter.join();
    initializer.join();

    // 只要不崩溃即视为通过
    PASS();
}

// 测试 11: IsInitialized 的内存可见性
void test_memory_visibility(TestStats& stats) {
    TEST_CASE("IsInitialized memory visibility");

    LazyValue<int> lazy;
    std::atomic<bool> observer_saw_initialized{false};

    std::thread writer([&lazy]() {
        lazy.GetOrInit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return 42;
        });
    });

    std::thread reader([&lazy, &observer_saw_initialized]() {
        // 轮询直到看到已初始化
        while (!lazy.IsInitialized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        observer_saw_initialized.store(true);
    });

    writer.join();
    reader.join();

    if (!observer_saw_initialized.load()) {
        FAIL("Observer did not see initialized state");
        return;
    }

    PASS();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  LazyValue Complete Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    TestStats stats;

    test_basic_initialization(stats);
    test_cache_hit(stats);
    test_thread_safety(stats);
    test_reset(stats);
    test_complex_type(stats);
    test_exception_safety(stats);
    test_reference_return(stats);
    test_move_semantics(stats);
    test_peek(stats);
    test_reset_concurrency(stats);
    test_memory_visibility(stats);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Results: " << stats.passed << " passed, "
              << stats.failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return stats.failed > 0 ? 1 : 0;
}
