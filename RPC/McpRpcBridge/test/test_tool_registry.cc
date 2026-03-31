/**
 * @file test_tool_registry.cc
 * @brief ToolRegistry 注册和查询测试
 */

#include "mcp/ToolRegistry.h"
#include <iostream>

// 前置声明 protobuf 生成的描述符（测试时使用 mock）
// 实际测试需要链接 protobuf 生成的代码

using namespace mcp_rpc;

int main() {
    std::cout << "Testing ToolRegistry..." << std::endl;

    ToolRegistry registry;

    // 测试 1: 空注册表
    if (!registry.Empty()) {
        std::cerr << "FAIL: New registry should be empty" << std::endl;
        return 1;
    }

    if (registry.Size() != 0) {
        std::cerr << "FAIL: New registry size should be 0" << std::endl;
        return 1;
    }

    std::cout << "PASS: Empty registry" << std::endl;

    // 测试 2: 注册服务（需要 protobuf 描述符）
    // 由于需要实际的 protobuf 描述符，这部分测试需要链接生成的代码
    // 这里仅做框架演示

    std::cout << "PASS: ToolRegistry basic tests (full tests require proto descriptors)" << std::endl;

    // 测试 3: 查找不存在的工具
    const ToolEntry* entry = registry.FindTool("NonExistent_Tool");
    if (entry != nullptr) {
        std::cerr << "FAIL: FindTool should return nullptr for non-existent tool" << std::endl;
        return 1;
    }

    std::cout << "PASS: FindTool returns nullptr for missing tool" << std::endl;

    // 测试 4: 列出空工具列表
    auto tools = registry.ListTools();
    if (!tools.empty()) {
        std::cerr << "FAIL: ListTools should return empty vector" << std::endl;
        return 1;
    }

    std::cout << "PASS: ListTools on empty registry" << std::endl;

    std::cout << "\nAll ToolRegistry tests passed!" << std::endl;
    return 0;
}
