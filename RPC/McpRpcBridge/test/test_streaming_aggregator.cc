/**
 * @file test_streaming_aggregator.cc
 * @brief StreamingResponseAggregator 流式聚合测试
 *
 * 测试覆盖：
 * - 空响应聚合
 * - 单消息聚合
 * - 多消息聚合（JSON 数组模式）
 * - 字符串连接模式
 * - 大小限制检查
 * - 错误处理
 * - 边缘情况
 */

#include "mcp/StreamingResponseAggregator.h"
#include "mcp/StreamingConfig.h"
#include <iostream>
#include <memory>
#include <cassert>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <nlohmann/json.hpp>

// 引入测试 proto
#include "test_service.pb.h"

using namespace mcp_rpc;

// 辅助函数：打印测试结果
void printTestResult(const std::string& test_name, bool passed) {
    std::cout << (passed ? "PASS" : "FAIL") << ": " << test_name << std::endl;
}

// 辅助函数：创建 StreamResponse 消息
std::unique_ptr<testproto::StreamResponse> CreateStreamResponse(const std::string& data) {
    auto msg = std::make_unique<testproto::StreamResponse>();
    msg->set_data(data);
    return msg;
}

// 辅助函数：序列化消息为字节
std::vector<uint8_t> SerializeMessage(const google::protobuf::Message& msg) {
    std::vector<uint8_t> bytes(msg.ByteSizeLong());
    msg.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()));
    return bytes;
}

int main() {
    std::cout << "=== StreamingResponseAggregator Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    // ============================================
    // 测试 1: 空消息列表聚合
    // ============================================
    std::cout << "Test 1: Empty message list aggregation" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 1024 * 1024;  // 1MB
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds", success);
        if (success) passed++; else failed++;

        bool empty_array = result.aggregated_json == "[]";
        printTestResult("Result is empty JSON array", empty_array);
        if (empty_array) passed++; else failed++;

        bool zero_count = result.message_count == 0;
        printTestResult("Message count is 0", zero_count);
        if (zero_count) passed++; else failed++;

        bool zero_bytes = result.total_bytes == 0;
        printTestResult("Total bytes is 0", zero_bytes);
        if (zero_bytes) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 2: 单消息聚合
    // ============================================
    std::cout << "Test 2: Single message aggregation" << std::endl;
    {
        StreamingConfig config;
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.push_back(CreateStreamResponse("hello"));

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds", success);
        if (success) passed++; else failed++;

        // 单消息应该返回单元素数组
        bool is_array = !result.aggregated_json.empty() &&
                        result.aggregated_json.front() == '[' &&
                        result.aggregated_json.back() == ']';
        printTestResult("Result is JSON array", is_array);
        if (is_array) passed++; else failed++;

        bool one_count = result.message_count == 1;
        printTestResult("Message count is 1", one_count);
        if (one_count) passed++; else failed++;

        // 验证 JSON 内容
        try {
            auto parsed = nlohmann::json::parse(result.aggregated_json);
            bool has_one_element = parsed.is_array() && parsed.size() == 1;
            printTestResult("Array has one element", has_one_element);
            if (has_one_element) passed++; else failed++;

            if (has_one_element) {
                bool correct_data = parsed[0]["data"] == "hello";
                printTestResult("Element data is correct", correct_data);
                if (correct_data) passed++; else failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL: Failed to parse JSON: " << e.what() << std::endl;
            failed += 2;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 3: 多消息聚合（JSON 数组模式）
    // ============================================
    std::cout << "Test 3: Multiple messages aggregation (JSON array mode)" << std::endl;
    {
        StreamingConfig config;
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.push_back(CreateStreamResponse("data1"));
        messages.push_back(CreateStreamResponse("data2"));
        messages.push_back(CreateStreamResponse("data3"));

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds", success);
        if (success) passed++; else failed++;

        bool three_count = result.message_count == 3;
        printTestResult("Message count is 3", three_count);
        if (three_count) passed++; else failed++;

        // 验证 JSON 内容
        try {
            auto parsed = nlohmann::json::parse(result.aggregated_json);
            bool has_three_elements = parsed.is_array() && parsed.size() == 3;
            printTestResult("Array has 3 elements", has_three_elements);
            if (has_three_elements) passed++; else failed++;

            if (has_three_elements) {
                bool correct_order = parsed[0]["data"] == "data1" &&
                                     parsed[1]["data"] == "data2" &&
                                     parsed[2]["data"] == "data3";
                printTestResult("Elements are in correct order", correct_order);
                if (correct_order) passed++; else failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL: Failed to parse JSON: " << e.what() << std::endl;
            failed += 2;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 4: 大小限制检查
    // ============================================
    std::cout << "Test 4: Size limit check" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 50;  // 很小的限制
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        // 创建多个较大的消息
        for (int i = 0; i < 10; ++i) {
            messages.push_back(CreateStreamResponse("large_data_chunk_" + std::to_string(i)));
        }

        auto result = aggregator.AggregateMessages(messages);

        // 应该因为超过大小限制而失败
        bool exceeded_limit = !result.success;
        printTestResult("Aggregation fails due to size limit", exceeded_limit);
        if (exceeded_limit) passed++; else failed++;

        if (!result.success) {
            bool has_error_message = !result.error_message.empty() &&
                                     result.error_message.find("exceeds limit") != std::string::npos;
            printTestResult("Error message mentions size limit", has_error_message);
            if (has_error_message) passed++; else failed++;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 5: WouldExceedLimit 辅助函数
    // ============================================
    std::cout << "Test 5: WouldExceedLimit helper function" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 100;
        StreamingResponseAggregator aggregator(config);

        bool would_not_exceed = !aggregator.WouldExceedLimit(50, 40);
        printTestResult("50 + 40 does not exceed 100", would_not_exceed);
        if (would_not_exceed) passed++; else failed++;

        bool would_exceed = aggregator.WouldExceedLimit(50, 60);
        printTestResult("50 + 60 exceeds 100", would_exceed);
        if (would_exceed) passed++; else failed++;

        bool would_exceed_exact = aggregator.WouldExceedLimit(50, 50);
        printTestResult("50 + 50 does not exceed (exact limit)", !would_exceed_exact);
        if (!would_exceed_exact) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 6: ListUsersResponse 聚合（复杂消息类型）
    // ============================================
    std::cout << "Test 6: Complex message type (ListUsersResponse) aggregation" << std::endl;
    {
        StreamingConfig config;
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;

        // 创建多个 ListUsersResponse
        for (int i = 0; i < 3; ++i) {
            auto msg = std::make_unique<testproto::ListUsersResponse>();
            msg->set_total(100);

            // 添加一些用户
            auto* user1 = msg->add_users();
            user1->set_id(i * 10 + 1);
            user1->set_name("User_" + std::to_string(i * 10 + 1));
            user1->set_email("user" + std::to_string(i * 10 + 1) + "@example.com");
            user1->set_active(true);

            auto* user2 = msg->add_users();
            user2->set_id(i * 10 + 2);
            user2->set_name("User_" + std::to_string(i * 10 + 2));
            user2->set_email("user" + std::to_string(i * 10 + 2) + "@example.com");
            user2->set_active(false);

            messages.push_back(std::move(msg));
        }

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds for complex type", success);
        if (success) passed++; else failed++;

        try {
            auto parsed = nlohmann::json::parse(result.aggregated_json);
            bool is_array = parsed.is_array();
            printTestResult("Result is JSON array", is_array);
            if (is_array) passed++; else failed++;

            bool has_three_elements = is_array && parsed.size() == 3;
            printTestResult("Array has 3 elements", has_three_elements);
            if (has_three_elements) passed++; else failed++;

            if (has_three_elements) {
                // 验证第一个元素的结构
                bool has_users = parsed[0].contains("users") && parsed[0]["users"].is_array();
                printTestResult("Element has users array", has_users);
                if (has_users) passed++; else failed++;

                bool has_total = parsed[0].contains("total") && parsed[0]["total"] == 100;
                printTestResult("Element has correct total", has_total);
                if (has_total) passed++; else failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL: Failed to parse JSON: " << e.what() << std::endl;
            failed += 3;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 7: GetConfig 方法
    // ============================================
    std::cout << "Test 7: GetConfig method" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 2 * 1024 * 1024;  // 2MB
        config.register_client_streaming = true;
        StreamingResponseAggregator aggregator(config);

        const auto& retrieved_config = aggregator.GetConfig();
        bool correct_limit = retrieved_config.aggregate_max_bytes == 2 * 1024 * 1024;
        printTestResult("Config has correct limit", correct_limit);
        if (correct_limit) passed++; else failed++;

        bool correct_flag = retrieved_config.register_client_streaming == true;
        printTestResult("Config has correct flag", correct_flag);
        if (correct_flag) passed++; else failed++;
    }
    std::cout << std::endl;

    // ============================================
    // 测试 8: 边缘情况 - 空字符串数据
    // ============================================
    std::cout << "Test 8: Edge case - empty string data" << std::endl;
    {
        StreamingConfig config;
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.push_back(CreateStreamResponse(""));
        messages.push_back(CreateStreamResponse("non-empty"));
        messages.push_back(CreateStreamResponse(""));

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds with empty strings", success);
        if (success) passed++; else failed++;

        // 注意：protobuf 的 JSON 序列化默认省略空值，所以空字符串的 data 字段不会出现在 JSON 中
        // 这是预期行为，不是 bug

        try {
            auto parsed = nlohmann::json::parse(result.aggregated_json);
            bool has_three_elements = parsed.is_array() && parsed.size() == 3;
            printTestResult("Array has 3 elements", has_three_elements);
            if (has_three_elements) passed++; else failed++;

            if (has_three_elements) {
                // 检查中间元素有正确的数据
                bool has_middle_data = parsed[1]["data"] == "non-empty";
                printTestResult("Middle element has correct data", has_middle_data);
                if (has_middle_data) passed++; else failed++;

                // 检查第一个和第三个元素是空对象（因为空字符串被 protobuf 省略）
                bool first_is_empty = parsed[0].is_object() && parsed[0].empty();
                printTestResult("First element is empty object (protobuf omits empty strings)", first_is_empty);
                if (first_is_empty) passed++; else failed++;

                bool third_is_empty = parsed[2].is_object() && parsed[2].empty();
                printTestResult("Third element is empty object (protobuf omits empty strings)", third_is_empty);
                if (third_is_empty) passed++; else failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL: Failed to parse JSON: " << e.what() << std::endl;
            failed += 3;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 9: 边缘情况 - 特殊字符
    // ============================================
    std::cout << "Test 9: Edge case - special characters" << std::endl;
    {
        StreamingConfig config;
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.push_back(CreateStreamResponse("hello\nworld"));
        messages.push_back(CreateStreamResponse("tab\there"));
        messages.push_back(CreateStreamResponse("quote\"test"));
        messages.push_back(CreateStreamResponse("unicode\u00e9"));

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds with special chars", success);
        if (success) passed++; else failed++;

        try {
            auto parsed = nlohmann::json::parse(result.aggregated_json);
            bool has_four_elements = parsed.is_array() && parsed.size() == 4;
            printTestResult("Array has 4 elements", has_four_elements);
            if (has_four_elements) passed++; else failed++;
        } catch (const std::exception& e) {
            std::cout << "FAIL: Failed to parse JSON: " << e.what() << std::endl;
            failed += 1;
        }
    }
    std::cout << std::endl;

    // ============================================
    // 测试 10: 大量小消息聚合
    // ============================================
    std::cout << "Test 10: Many small messages aggregation" << std::endl;
    {
        StreamingConfig config;
        config.aggregate_max_bytes = 10 * 1024;  // 10KB
        StreamingResponseAggregator aggregator(config);

        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        // 创建 50 个小消息
        for (int i = 0; i < 50; ++i) {
            messages.push_back(CreateStreamResponse("msg_" + std::to_string(i)));
        }

        auto result = aggregator.AggregateMessages(messages);

        bool success = result.success;
        printTestResult("Aggregation succeeds for 50 messages", success);
        if (success) passed++; else failed++;

        bool correct_count = result.message_count == 50;
        printTestResult("Message count is 50", correct_count);
        if (correct_count) passed++; else failed++;

        // 验证总字节数
        bool has_bytes = result.total_bytes > 0;
        printTestResult("Total bytes > 0", has_bytes);
        if (has_bytes) passed++; else failed++;
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
        std::cout << "All StreamingResponseAggregator tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}
