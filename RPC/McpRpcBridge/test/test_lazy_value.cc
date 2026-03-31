/**
 * @file test_lazy_value.cc
 * @brief LazyValue 线程安全延迟初始化测试
 */

#include "mcp/LazyValue.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace mcp_rpc;

int main() {
    std::cout << "Testing LazyValue..." << std::endl;

    // 测试 1: 基本延迟初始化
    {
        LazyValue<int> lazy;
        if (lazy.IsInitialized()) {
            std::cerr << "FAIL: LazyValue should not be initialized" << std::endl;
            return 1;
        }

        int value = lazy.GetOrInit([]() { return 42; });
        if (value != 42) {
            std::cerr << "FAIL: Expected 42, got " << value << std::endl;
            return 1;
        }

        if (!lazy.IsInitialized()) {
            std::cerr << "FAIL: LazyValue should be initialized" << std::endl;
            return 1;
        }

        std::cout << "PASS: Basic lazy initialization" << std::endl;
    }

    // 测试 2: 缓存命中（初始化函数只调用一次）
    {
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

        if (v1 != 100 || v2 != 100) {
            std::cerr << "FAIL: Cached value mismatch" << std::endl;
            return 1;
        }

        if (call_count != 1) {
            std::cerr << "FAIL: Init function called " << call_count << " times, expected 1" << std::endl;
            return 1;
        }

        std::cout << "PASS: Cache hit (init called once)" << std::endl;
    }

    // 测试 3: 并发安全
    {
        LazyValue<int> lazy;
        std::atomic<int> call_count{0};

        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&lazy, &call_count]() {
                lazy.GetOrInit([&call_count]() {
                    call_count++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    return 999;
                });
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        if (call_count != 1) {
            std::cerr << "FAIL: Concurrent safety violated, init called " << call_count << " times" << std::endl;
            return 1;
        }

        std::cout << "PASS: Thread-safe initialization" << std::endl;
    }

    std::cout << "\nAll LazyValue tests passed!" << std::endl;
    return 0;
}
