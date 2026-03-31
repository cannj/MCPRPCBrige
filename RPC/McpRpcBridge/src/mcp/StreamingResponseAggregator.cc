#include "mcp/StreamingResponseAggregator.h"
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/dynamic_message.h>
#include <sstream>

namespace mcp_rpc {

StreamingResponseAggregator::StreamingResponseAggregator(const StreamingConfig& config)
    : config_(config) {}

bool StreamingResponseAggregator::WouldExceedLimit(size_t current_bytes, size_t new_bytes) const {
    return (current_bytes + new_bytes) > config_.aggregate_max_bytes;
}

bool StreamingResponseAggregator::IsStringType(const google::protobuf::Descriptor* type) const {
    if (!type) return false;

    // 检查是否是 protobuf 的 StringValue 类型（google.protobuf.StringValue）
    // 这种类型的全名是 ".google.protobuf.StringValue"
    if (type->full_name() == "google.protobuf.StringValue" ||
        type->full_name() == ".google.protobuf.StringValue") {
        return true;
    }

    // 或者检查是否只有一个 string 字段且字段名不是 "data" 这种通用名
    // 更严格地说，我们只对真正的包装器类型使用 CONCAT 模式
    if (type->field_count() == 1) {
        const google::protobuf::FieldDescriptor* field = type->field(0);
        // 只有当字段是 string 类型且没有标签（不是消息类型）时才考虑
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_STRING) {
            // 对于像 StreamResponse 这样的消息，我们不应该使用 CONCAT 模式
            // 只有真正的纯字符串包装器才使用 CONCAT
            // 这里我们通过检查消息类型名称来判断
            std::string type_name = type->name();
            if (type_name == "StringValue" || type_name == "StringProto") {
                return true;
            }
        }
    }
    return false;
}

std::string StreamingResponseAggregator::MessageBytesToJson(
    const std::vector<uint8_t>& bytes,
    const google::protobuf::Descriptor* type,
    const google::protobuf::util::JsonPrintOptions& options) {

    if (bytes.empty() || !type) {
        return "";
    }

    // 创建动态消息
    google::protobuf::DynamicMessageFactory factory;
    const google::protobuf::Message* prototype = factory.GetPrototype(type);
    if (!prototype) {
        return "";
    }

    std::unique_ptr<google::protobuf::Message> message(prototype->New());
    if (!message->ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
        return "";
    }

    std::string json_str;
    auto status = google::protobuf::util::MessageToJsonString(*message, &json_str, options);
    if (!status.ok()) {
        return "";
    }

    return json_str;
}

AggregationResult StreamingResponseAggregator::Aggregate(
    const std::vector<std::vector<uint8_t>>& messages,
    const google::protobuf::Descriptor* response_type,
    std::function<std::string(const std::vector<uint8_t>&)> to_json_fn) {

    AggregationResult result;

    // 边缘情况：空消息列表
    if (messages.empty()) {
        result.success = true;
        result.aggregated_json = "[]";
        result.message_count = 0;
        result.total_bytes = 0;
        return result;
    }

    // 确定聚合模式
    StreamingConfig::AggregateMode mode = StreamingConfig::AggregateMode::JSON_ARRAY;

    // 如果是 string 类型且消息较多，使用 CONCAT 模式
    if (IsStringType(response_type) && messages.size() > 1) {
        mode = StreamingConfig::AggregateMode::CONCAT;
    }

    result.message_count = static_cast<int>(messages.size());

    // 聚合逻辑
    std::vector<std::string> json_items;
    std::string concatenated_string;
    size_t total_bytes = 0;

    for (const auto& msg_bytes : messages) {
        // 检查大小限制
        if (WouldExceedLimit(total_bytes, msg_bytes.size())) {
            result.success = false;
            result.error_message = "Aggregated response size (" +
                                   std::to_string(total_bytes + msg_bytes.size()) +
                                   " bytes) exceeds limit (" +
                                   std::to_string(config_.aggregate_max_bytes) + " bytes)";
            return result;
        }

        total_bytes += msg_bytes.size();

        // 转换消息为 JSON
        std::string json_str;
        if (to_json_fn) {
            json_str = to_json_fn(msg_bytes);
        } else {
            json_str = MessageBytesToJson(msg_bytes, response_type, {});
        }

        if (json_str.empty() && !msg_bytes.empty()) {
            result.success = false;
            result.error_message = "Failed to serialize message to JSON";
            return result;
        }

        if (mode == StreamingConfig::AggregateMode::CONCAT) {
            // 对于 CONCAT 模式，提取字符串值（去掉 JSON 引号）
            if (!json_str.empty()) {
                // JSON 字符串格式为 "value"，提取中间部分
                if (json_str.size() >= 2 && json_str.front() == '"' && json_str.back() == '"') {
                    concatenated_string += json_str.substr(1, json_str.size() - 2);
                } else {
                    concatenated_string += json_str;
                }
            }
        } else {
            json_items.push_back(json_str);
        }
    }

    result.total_bytes = total_bytes;
    result.success = true;

    // 构建最终 JSON
    if (mode == StreamingConfig::AggregateMode::CONCAT) {
        // 返回连接的字符串（JSON 格式）
        result.aggregated_json = "\"" + concatenated_string + "\"";
    } else {
        // 返回 JSON 数组
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < json_items.size(); ++i) {
            if (i > 0) oss << ",";
            oss << json_items[i];
        }
        oss << "]";
        result.aggregated_json = oss.str();
    }

    return result;
}

AggregationResult StreamingResponseAggregator::AggregateMessages(
    const std::vector<std::unique_ptr<google::protobuf::Message>>& messages,
    const google::protobuf::util::JsonPrintOptions& json_options) {

    AggregationResult result;

    // 边缘情况：空消息列表
    if (messages.empty()) {
        result.success = true;
        result.aggregated_json = "[]";
        result.message_count = 0;
        result.total_bytes = 0;
        return result;
    }

    // 获取响应类型
    const google::protobuf::Descriptor* response_type = messages[0]->GetDescriptor();

    // 确定聚合模式
    StreamingConfig::AggregateMode mode = StreamingConfig::AggregateMode::JSON_ARRAY;
    if (IsStringType(response_type) && messages.size() > 1) {
        mode = StreamingConfig::AggregateMode::CONCAT;
    }

    result.message_count = static_cast<int>(messages.size());

    // 聚合逻辑
    std::vector<std::string> json_items;
    std::string concatenated_string;
    size_t total_bytes = 0;

    for (const auto& message : messages) {
        if (!message) continue;

        // 估算字节数（使用 ByteSizeLong）
        size_t msg_bytes = message->ByteSizeLong();

        // 检查大小限制
        if (WouldExceedLimit(total_bytes, msg_bytes)) {
            result.success = false;
            result.error_message = "Aggregated response size (" +
                                   std::to_string(total_bytes + msg_bytes) +
                                   " bytes) exceeds limit (" +
                                   std::to_string(config_.aggregate_max_bytes) + " bytes)";
            return result;
        }

        total_bytes += msg_bytes;

        // 转换消息为 JSON
        std::string json_str;
        auto status = google::protobuf::util::MessageToJsonString(*message, &json_str, json_options);
        if (!status.ok()) {
            result.success = false;
            result.error_message = "Failed to serialize message to JSON: " + status.ToString();
            return result;
        }

        if (mode == StreamingConfig::AggregateMode::CONCAT) {
            // 对于 CONCAT 模式，提取字符串值
            if (!json_str.empty()) {
                if (json_str.size() >= 2 && json_str.front() == '"' && json_str.back() == '"') {
                    concatenated_string += json_str.substr(1, json_str.size() - 2);
                } else {
                    concatenated_string += json_str;
                }
            }
        } else {
            json_items.push_back(json_str);
        }
    }

    result.total_bytes = total_bytes;
    result.success = true;

    // 构建最终 JSON
    if (mode == StreamingConfig::AggregateMode::CONCAT) {
        result.aggregated_json = "\"" + concatenated_string + "\"";
    } else {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < json_items.size(); ++i) {
            if (i > 0) oss << ",";
            oss << json_items[i];
        }
        oss << "]";
        result.aggregated_json = oss.str();
    }

    return result;
}

} // namespace mcp_rpc
