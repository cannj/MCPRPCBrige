/**
 * @file test_arguments_deserializer.cc
 * @brief ArgumentsDeserializer JSON 到 Protobuf 反序列化测试
 */

#include "mcp/ArgumentsDeserializer.h"
#include "test_service.pb.h"
#include <iostream>
#include <cassert>
#include <functional>
#include <cstring>

using namespace mcp_rpc;

// 辅助函数：打印测试结果
void printTestResult(const std::string& test_name, bool passed) {
    std::cout << (passed ? "PASS" : "FAIL") << ": " << test_name << std::endl;
}

// 辅助函数：检查两个字节数组是否相等
bool compareByteArrays(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

int main() {
    std::cout << "=== ArgumentsDeserializer Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    // ============================================
    // 测试 1: 基本反序列化 - LoginRequest
    // ============================================
    std::cout << "Test 1: Basic deserialization (LoginRequest)" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest request;

        std::string json = R"({
            "username": "testuser",
            "password": "secret123"
        })";

        std::string error = deserializer.Deserialize(json, &request);

        bool success = error.empty();
        printTestResult("Deserialization succeeds", success);
        if (success) passed++; else failed++;

        if (success) {
            bool username_correct = request.username() == "testuser";
            printTestResult("username field correct", username_correct);
            if (username_correct) passed++; else failed++;

            bool password_correct = request.password() == "secret123";
            printTestResult("password field correct", password_correct);
            if (password_correct) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 2: 未知字段容错（默认开启）
    // ============================================
    std::cout << "Test 2: Unknown field tolerance (default: ignore)" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest request;

        // LLM 可能传入额外参数
        std::string json = R"({
            "username": "testuser",
            "password": "secret123",
            "unknown_field": "should be ignored",
            "extra_number": 42,
            "nested_unknown": {
                "foo": "bar"
            }
        })";

        std::string error = deserializer.Deserialize(json, &request);

        // 默认应该忽略未知字段，不报错
        bool success = error.empty();
        printTestResult("Unknown fields ignored (no error)", success);
        if (success) passed++; else failed++;

        if (success) {
            bool username_correct = request.username() == "testuser";
            printTestResult("username still parsed correctly", username_correct);
            if (username_correct) passed++; else failed++;

            bool password_correct = request.password() == "secret123";
            printTestResult("password still parsed correctly", password_correct);
            if (password_correct) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 3: 未知字段报错模式
    // ============================================
    std::cout << "Test 3: Unknown field error mode" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        deserializer.SetIgnoreUnknownFields(false);  // 设置为报错模式

        testproto::LoginRequest request;

        std::string json = R"({
            "username": "testuser",
            "password": "secret123",
            "unknown_field": "should cause error"
        })";

        std::string error = deserializer.Deserialize(json, &request);

        // 应该报错
        bool should_error = !error.empty();
        printTestResult("Unknown field causes error", should_error);
        if (should_error) passed++; else failed++;

        if (should_error) {
            std::cout << "  Error message: " << error << std::endl;
            bool error_mentions_field = error.find("unknown_field") != std::string::npos ||
                                         error.find("unknown") != std::string::npos;
            printTestResult("Error message mentions unknown field", error_mentions_field);
            if (error_mentions_field) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 4: 嵌套消息反序列化
    // ============================================
    std::cout << "Test 4: Nested message deserialization" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::GetUserResponse response;

        std::string json = R"({
            "user": {
                "id": 12345,
                "name": "John Doe",
                "email": "john@example.com",
                "active": true
            }
        })";

        std::string error = deserializer.Deserialize(json, &response);

        bool success = error.empty();
        printTestResult("Nested message deserialization succeeds", success);
        if (success) passed++; else failed++;

        if (success) {
            bool id_correct = response.user().id() == 12345;
            printTestResult("nested user.id correct", id_correct);
            if (id_correct) passed++; else failed++;

            bool name_correct = response.user().name() == "John Doe";
            printTestResult("nested user.name correct", name_correct);
            if (name_correct) passed++; else failed++;

            bool email_correct = response.user().email() == "john@example.com";
            printTestResult("nested user.email correct", email_correct);
            if (email_correct) passed++; else failed++;

            bool active_correct = response.user().active() == true;
            printTestResult("nested user.active correct", active_correct);
            if (active_correct) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 5: repeated 字段反序列化
    // ============================================
    std::cout << "Test 5: Repeated field deserialization" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::ListUsersResponse response;

        std::string json = R"({
            "users": [
                {"id": 1, "name": "Alice", "email": "alice@example.com", "active": true},
                {"id": 2, "name": "Bob", "email": "bob@example.com", "active": false},
                {"id": 3, "name": "Charlie", "email": "charlie@example.com", "active": true}
            ],
            "total": 3
        })";

        std::string error = deserializer.Deserialize(json, &response);

        bool success = error.empty();
        printTestResult("Repeated field deserialization succeeds", success);
        if (success) passed++; else failed++;

        if (success) {
            bool total_correct = response.total() == 3;
            printTestResult("total field correct", total_correct);
            if (total_correct) passed++; else failed++;

            bool users_size_correct = response.users_size() == 3;
            printTestResult("users array size correct", users_size_correct);
            if (users_size_correct) passed++; else failed++;

            if (users_size_correct) {
                bool first_user_name = response.users(0).name() == "Alice";
                printTestResult("first user name correct", first_user_name);
                if (first_user_name) passed++; else failed++;

                bool second_user_name = response.users(1).name() == "Bob";
                printTestResult("second user name correct", second_user_name);
                if (second_user_name) passed++; else failed++;
            }
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 6: 嵌套未知字段容错
    // ============================================
    std::cout << "Test 6: Nested unknown field tolerance" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::GetUserResponse response;

        // 嵌套消息中包含未知字段
        std::string json = R"({
            "user": {
                "id": 999,
                "name": "Test User",
                "email": "test@example.com",
                "unknown_nested_field": "should be ignored",
                "another_extra": 12345
            },
            "unknown_top_level": "also ignored"
        })";

        std::string error = deserializer.Deserialize(json, &response);

        bool success = error.empty();
        printTestResult("Nested unknown fields ignored", success);
        if (success) passed++; else failed++;

        if (success) {
            bool id_correct = response.user().id() == 999;
            printTestResult("Known fields still parsed", id_correct);
            if (id_correct) passed++; else failed++;

            bool name_correct = response.user().name() == "Test User";
            printTestResult("Nested known fields parsed", name_correct);
            if (name_correct) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 7: DeserializeToBytes 基本功能
    // ============================================
    std::cout << "Test 7: DeserializeToBytes basic functionality" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest prototype;
        std::vector<uint8_t> output;

        std::string json = R"({
            "username": "binaryuser",
            "password": "binarypass"
        })";

        std::string error = deserializer.DeserializeToBytes(json, prototype, output);

        bool success = error.empty() && !output.empty();
        printTestResult("DeserializeToBytes succeeds", success);
        if (success) passed++; else failed++;

        if (success) {
            // 验证二进制数据可以正确反序列化回来
            testproto::LoginRequest parsed;
            bool parse_from_array = parsed.ParseFromArray(output.data(), output.size());
            printTestResult("Binary data can be parsed back", parse_from_array);
            if (parse_from_array) passed++; else failed++;

            if (parse_from_array) {
                bool username_match = parsed.username() == "binaryuser";
                printTestResult("Username matches after binary roundtrip", username_match);
                if (username_match) passed++; else failed++;

                bool password_match = parsed.password() == "binarypass";
                printTestResult("Password matches after binary roundtrip", password_match);
                if (password_match) passed++; else failed++;
            }
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 8: DeserializeToBytes 未知字段容错
    // ============================================
    std::cout << "Test 8: DeserializeToBytes unknown field tolerance" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest prototype;
        std::vector<uint8_t> output;

        std::string json = R"({
            "username": "toleranceuser",
            "password": "tolerancepass",
            "unknown_extra": "should be ignored in binary too"
        })";

        std::string error = deserializer.DeserializeToBytes(json, prototype, output);

        // 未知字段应该被忽略，转换成功
        bool success = error.empty() && !output.empty();
        printTestResult("DeserializeToBytes ignores unknown fields", success);
        if (success) passed++; else failed++;

        if (success) {
            // 验证二进制数据可以正确反序列化回来
            testproto::LoginRequest parsed;
            bool parse_from_array = parsed.ParseFromArray(output.data(), output.size());
            printTestResult("Binary data parses back correctly", parse_from_array);
            if (parse_from_array) passed++; else failed++;

            if (parse_from_array) {
                bool username_match = parsed.username() == "toleranceuser";
                printTestResult("Username correct in binary output", username_match);
                if (username_match) passed++; else failed++;
            }
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 9: 空 JSON 对象
    // ============================================
    std::cout << "Test 9: Empty JSON object" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest request;

        std::string json = R"({})";

        std::string error = deserializer.Deserialize(json, &request);

        bool success = error.empty();
        printTestResult("Empty JSON object succeeds", success);
        if (success) passed++; else failed++;

        if (success) {
            bool username_empty = request.username().empty();
            printTestResult("username is default (empty)", username_empty);
            if (username_empty) passed++; else failed++;

            bool password_empty = request.password().empty();
            printTestResult("password is default (empty)", password_empty);
            if (password_empty) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 10: 类型错误检测
    // ============================================
    // 注意：Protobuf JSON 解析对类型错误有一定容错性
    // 例如字符串 "123" 可以转为 int32
    std::cout << "Test 10: Type coercion behavior" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::GetUserRequest request;

        // Protobuf 可能允许数字字符串转为 int32
        std::string json = R"({"user_id": 42})";

        std::string error = deserializer.Deserialize(json, &request);

        bool success = error.empty();
        printTestResult("Valid type parses successfully", success);
        if (success) passed++; else failed++;

        if (success) {
            bool id_correct = request.user_id() == 42;
            printTestResult("user_id value correct", id_correct);
            if (id_correct) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 11: 空字符串输入
    // ============================================
    std::cout << "Test 11: Empty string input" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest request;

        std::string json = "";

        std::string error = deserializer.Deserialize(json, &request);

        // 空字符串应该会报错（不是有效的 JSON）
        bool should_error = !error.empty();
        printTestResult("Empty string causes error", should_error);
        if (should_error) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 12: 无效 JSON 格式
    // ============================================
    std::cout << "Test 12: Invalid JSON format" << std::endl;
    {
        ArgumentsDeserializer deserializer;
        testproto::LoginRequest request;

        std::string json = R"({"username": "test", "password": "secret" )";  // 缺少右括号

        std::string error = deserializer.Deserialize(json, &request);

        bool should_error = !error.empty();
        printTestResult("Invalid JSON causes error", should_error);
        if (should_error) passed++; else failed++;

        if (should_error) {
            std::cout << "  Error message: " << error << std::endl;
        }
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
        std::cout << "All ArgumentsDeserializer tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
