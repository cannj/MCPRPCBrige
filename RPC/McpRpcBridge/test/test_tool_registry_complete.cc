/**
 * @file test_tool_registry_complete.cc
 * @brief ToolRegistry 完整单元测试
 *
 * 测试覆盖：
 * 1. 空注册表状态
 * 2. 服务注册（使用测试 proto）
 * 3. 工具名称生成
 * 4. 工具查找
 * 5. 工具列表
 * 6. 流式 RPC 标记
 * 7. ToolEntry 的 LazyValue Schema 缓存
 * 8. 多个服务注册
 * 9. 描述生成
 */

#include "mcp/ToolRegistry.h"
#include "test_service.pb.h"  // 由 test_service.proto 生成，位于 build/test/

#include <iostream>
#include <set>
#include <google/protobuf/descriptor.h>

using namespace mcp_rpc;

// 测试计数器
struct TestStats {
    int passed = 0;
    int failed = 0;
};

// 辅助函数：获取服务描述符
inline const google::protobuf::ServiceDescriptor* GetTestUserService() {
    return testproto::TestUserServiceRpc::descriptor();
}

inline const google::protobuf::ServiceDescriptor* GetTestOrderService() {
    return testproto::TestOrderServiceRpc::descriptor();
}

#define TEST_CASE(name) std::cout << "Running: " << name << "... ";
#define PASS() do { stats.passed++; std::cout << "PASS" << std::endl; } while(0)
#define FAIL(msg) do { stats.failed++; std::cerr << "FAIL: " << msg << std::endl; } while(0)

// 测试 1: 空注册表
void test_empty_registry(TestStats& stats) {
    TEST_CASE("Empty registry");

    ToolRegistry registry;

    if (!registry.Empty()) {
        FAIL("New registry should be empty");
        return;
    }

    if (registry.Size() != 0) {
        FAIL("New registry size should be 0");
        return;
    }

    PASS();
}

// 测试 2: 注册服务
void test_register_service(TestStats& stats) {
    TEST_CASE("Register service");

    ToolRegistry registry;

    registry.RegisterService(GetTestUserService());

    // 应该有 6 个方法（Login, GetUser, CreateUser, DeleteUser, ListUsers, StreamData）
    size_t expected = 6;
    if (registry.Size() != expected) {
        FAIL("Expected " + std::to_string(expected) + " tools, got " +
             std::to_string(registry.Size()));
        return;
    }

    PASS();
}

// 测试 3: 工具名称生成
void test_tool_naming(TestStats& stats) {
    TEST_CASE("Tool naming convention");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    // 检查工具名称格式
    const ToolEntry* login = registry.FindTool("TestUserServiceRpc_Login");
    if (!login) {
        FAIL("Login tool not found");
        return;
    }

    // 验证名称和方法描述符匹配
    if (login->method->name() != "Login") {
        FAIL("Method name mismatch");
        return;
    }

    // 检查长名称截断（如果有）
    // TestUserServiceRpc 的服务名 + 方法名应该不超过 48 字符
    std::string fullName = login->method->service()->name() + "_" + login->method->name();
    if (fullName.size() > 48 && login->name.size() > 48) {
        FAIL("Long name truncation failed");
        return;
    }

    PASS();
}

// 测试 4: 工具查找
void test_tool_lookup(TestStats& stats) {
    TEST_CASE("Tool lookup");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    // 查找存在的工具
    const ToolEntry* login = registry.FindTool("TestUserServiceRpc_Login");
    if (!login) {
        FAIL("Login tool not found");
        return;
    }

    // 查找不存在的工具
    const ToolEntry* nonexistent = registry.FindTool("NonExistent_Tool");
    if (nonexistent) {
        FAIL("NonExistent_Tool should return nullptr");
        return;
    }

    // 查找所有工具
    std::vector<std::string> expected_tools = {
        "TestUserServiceRpc_Login",
        "TestUserServiceRpc_GetUser",
        "TestUserServiceRpc_CreateUser",
        "TestUserServiceRpc_DeleteUser",
        "TestUserServiceRpc_ListUsers",
        "TestUserServiceRpc_StreamData"
    };

    for (const auto& tool_name : expected_tools) {
        const ToolEntry* tool = registry.FindTool(tool_name);
        if (!tool) {
            FAIL("Tool not found: " + tool_name);
            return;
        }
    }

    PASS();
}

// 测试 5: 工具列表
void test_list_tools(TestStats& stats) {
    TEST_CASE("List tools");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    auto tools = registry.ListTools();

    if (tools.size() != registry.Size()) {
        FAIL("ListTools size mismatch");
        return;
    }

    // 检查返回的工具指针都有效
    std::set<std::string> names;
    for (const auto* tool : tools) {
        if (!tool) {
            FAIL("Null tool in list");
            return;
        }
        if (names.count(tool->name)) {
            FAIL("Duplicate tool name: " + tool->name);
            return;
        }
        names.insert(tool->name);
    }

    PASS();
}

// 测试 6: 流式 RPC 标记
void test_streaming_mark(TestStats& stats) {
    TEST_CASE("Streaming RPC mark");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    // StreamData 是 server-streaming RPC
    const ToolEntry* streamTool = registry.FindTool("TestUserServiceRpc_StreamData");
    if (!streamTool) {
        FAIL("StreamData tool not found");
        return;
    }

    // 检查描述中包含 [streaming] 标记
    if (streamTool->description.find("[streaming]") == std::string::npos) {
        FAIL("Streaming RPC should have [streaming] in description");
        return;
    }

    // 检查非流式 RPC 没有标记
    const ToolEntry* loginTool = registry.FindTool("TestUserServiceRpc_Login");
    if (loginTool->description.find("[streaming]") != std::string::npos) {
        FAIL("Non-streaming RPC should not have [streaming] in description");
        return;
    }

    PASS();
}

// 测试 7: ToolEntry 的 LazyValue Schema 缓存
void test_tool_entry_schema_cache(TestStats& stats) {
    TEST_CASE("ToolEntry schema cache");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    const ToolEntry* login = registry.FindTool("TestUserServiceRpc_Login");
    if (!login) {
        FAIL("Login tool not found");
        return;
    }

    // 初始时 schema 应该未初始化
    if (login->schema.IsInitialized()) {
        FAIL("Schema should not be initialized initially");
        return;
    }

    // 第一次调用 GetInputSchema 触发初始化
    const auto& schema1 = login->GetInputSchema();

    if (!login->schema.IsInitialized()) {
        FAIL("Schema should be initialized after GetInputSchema");
        return;
    }

    // 检查 schema 包含预期字段
    if (!schema1.contains("$ref")) {
        FAIL("Schema should contain $ref");
        return;
    }

    // 第二次调用应该返回缓存值
    const auto& schema2 = login->GetInputSchema();

    // 验证返回的是同一个对象（引用）
    if (&schema1 != &schema2) {
        FAIL("Schema should be cached (same reference)");
        return;
    }

    PASS();
}

// 测试 8: 多个服务注册
void test_multiple_services(TestStats& stats) {
    TEST_CASE("Multiple services registration");

    ToolRegistry registry;

    // 注册第一个服务
    const auto* descriptor1 = GetTestUserService();
    registry.RegisterService(descriptor1);
    size_t count1 = registry.Size();

    // 注册第二个服务
    const auto* descriptor2 = GetTestOrderService();
    registry.RegisterService(descriptor2);
    size_t count2 = registry.Size();

    // 第二个服务有 2 个方法
    if (count2 != count1 + 2) {
        FAIL("Expected " + std::to_string(count1 + 2) + " tools, got " +
             std::to_string(count2));
        return;
    }

    // 验证两个服务的工具都可以找到
    const ToolEntry* userTool = registry.FindTool("TestUserServiceRpc_Login");
    const ToolEntry* orderTool = registry.FindTool("TestOrderServiceRpc_GetOrder");

    if (!userTool || !orderTool) {
        FAIL("Tools from both services should be findable");
        return;
    }

    PASS();
}

// 测试 9: 描述生成
void test_description_generation(TestStats& stats) {
    TEST_CASE("Description generation");

    ToolRegistry registry;
    const auto* descriptor = GetTestUserService();
    registry.RegisterService(descriptor);

    const ToolEntry* login = registry.FindTool("TestUserServiceRpc_Login");
    if (!login) {
        FAIL("Login tool not found");
        return;
    }

    // 检查描述包含必要信息
    if (login->description.find("[RPC]") == std::string::npos) {
        FAIL("Description should contain [RPC] marker");
        return;
    }

    if (login->description.find("input:") == std::string::npos) {
        FAIL("Description should contain input type info");
        return;
    }

    if (login->description.find("output:") == std::string::npos) {
        FAIL("Description should contain output type info");
        return;
    }

    PASS();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ToolRegistry Complete Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    TestStats stats;

    test_empty_registry(stats);
    test_register_service(stats);
    test_tool_naming(stats);
    test_tool_lookup(stats);
    test_list_tools(stats);
    test_streaming_mark(stats);
    test_tool_entry_schema_cache(stats);
    test_multiple_services(stats);
    test_description_generation(stats);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Results: " << stats.passed << " passed, "
              << stats.failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return stats.failed > 0 ? 1 : 0;
}
