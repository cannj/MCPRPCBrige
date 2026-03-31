#include "mcp/MCPSession.h"
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/dynamic_message.h>
#include <iostream>

namespace mcp_rpc {

MCPSession::MCPSession(const ToolRegistry& registry,
                       std::shared_ptr<RpcInvoker> invoker,
                       StreamingConfig config)
    : registry_(registry)
    , invoker_(std::move(invoker))
    , config_(std::move(config))
    , state_(SessionState::New)
    , deserializer_()
    , aggregator_(config_) {}

nlohmann::json MCPSession::HandleRequest(const nlohmann::json& request) {
    try {
        // 解析 JSON-RPC 请求
        std::string method = request.value("method", "");
        nlohmann::json params = request.value("params", nlohmann::json::object());

        // JSON-RPC 2.0 notification（没有 id）不应该返回响应
        bool is_notification = !request.contains("id");
        nlohmann::json id = is_notification ? nlohmann::json() : request["id"];

        // 路由到对应的处理方法
        if (method == "initialize") {
            return HandleInitialize(params, id, is_notification);
        } else if (method == "tools/list") {
            if (state_ != SessionState::Initialized) {
                return MakeError(kInvalidRequest, "Session not initialized. Call 'initialize' first.", id);
            }
            return HandleToolsList(params, id, is_notification);
        } else if (method == "tools/call") {
            if (state_ != SessionState::Initialized) {
                return MakeError(kInvalidRequest, "Session not initialized. Call 'initialize' first.", id);
            }
            return HandleToolsCall(params, id, is_notification);
        } else {
            return MakeError(kMethodNotFound, "Unknown method: " + method, id);
        }

    } catch (const std::exception& e) {
        return MakeError(kInternalError, std::string("Internal error: ") + e.what(),
                         nlohmann::json());
    }
}

nlohmann::json MCPSession::HandleInitialize(const nlohmann::json& params, const nlohmann::json& id, bool is_notification) {
    state_ = SessionState::Initialized;

    // Notification 不返回响应
    if (is_notification) {
        return nlohmann::json();
    }

    nlohmann::json result = {
        {"protocolVersion", "2024-11-05"},
        {"serverInfo", {
            {"name", "MCP-RPC Bridge"},
            {"version", "1.0.0"}
        }},
        {"capabilities", {
            {"tools", {}}
        }}
    };

    return MakeSuccess(result, id);
}

nlohmann::json MCPSession::HandleToolsList(const nlohmann::json& params, const nlohmann::json& id, bool is_notification) {
    // Notification 不返回响应
    if (is_notification) {
        return nlohmann::json();
    }

    nlohmann::json tools = nlohmann::json::array();

    for (const auto* entry : registry_.ListTools()) {
        nlohmann::json tool = {
            {"name", entry->name},
            {"description", entry->description},
            {"inputSchema", entry->GetInputSchema()}
        };
        tools.push_back(tool);
    }

    return MakeSuccess({{"tools", tools}}, id);
}

nlohmann::json MCPSession::HandleToolsCall(const nlohmann::json& params, const nlohmann::json& id, bool is_notification) {
    // Notification 不返回响应
    if (is_notification) {
        return nlohmann::json();
    }

    std::string tool_name = params.value("name", "");
    nlohmann::json arguments = params.value("arguments", nlohmann::json::object());

    // 查找工具
    const ToolEntry* tool = registry_.FindTool(tool_name);
    if (!tool) {
        // 返回相似的工具名称，帮助 LLM 自我纠正
        std::vector<std::string> available;
        for (const auto* entry : registry_.ListTools()) {
            available.push_back(entry->name);
        }
        std::string available_str = available.empty() ? "none" : available[0];
        for (size_t i = 1; i < available.size() && i < 5; ++i) {
            available_str += ", " + available[i];
        }
        return MakeError(kMethodNotFound,
                         "Unknown tool: " + tool_name + ". Available: " + available_str,
                         params.value("id", nullptr));
    }

    // 将参数序列化为 JSON 字符串
    std::string args_json = arguments.dump();

    // 反序列化为 Protobuf 二进制
    std::vector<uint8_t> request_bytes;
    // 使用 DynamicMessageFactory 创建消息原型
    google::protobuf::DynamicMessageFactory factory;
    const google::protobuf::Message* prototype = factory.GetPrototype(tool->method->input_type());
    if (!prototype) {
        return MakeError(kInternalError, "Failed to get message prototype",
                         params.value("id", nullptr));
    }
    std::string error = deserializer_.DeserializeToBytes(
        args_json,
        *prototype,
        request_bytes);

    if (!error.empty()) {
        return MakeError(kInvalidParams, "Failed to parse arguments: " + error,
                         params.value("id", nullptr));
    }

    // 检查是否为流式 RPC
    bool is_streaming = tool->method->server_streaming() || tool->method->client_streaming();

    if (tool->method->client_streaming()) {
        if (!config_.register_client_streaming) {
            return MakeError(kInvalidParams,
                             "Client-streaming RPCs are not supported by MCP protocol",
                             params.value("id", nullptr));
        }
    }

    // 调用 RPC
    auto future = invoker_->Invoke(tool->method->full_name(), request_bytes);

    // 等待响应（同步）
    try {
        auto response_status = future.get();
        if (!response_status.ok()) {
            return MakeError(kInternalError, "RPC call failed: " + response_status.message(),
                             params.value("id", nullptr));
        }

        // 处理流式响应
        if (is_streaming && tool->method->server_streaming()) {
            return HandleStreamingRpc(tool, response_status.value(), params.value("id", nullptr));
        }

        // 处理普通响应
        const auto& response_bytes = response_status.value();
        if (response_bytes.empty()) {
            return MakeSuccess({{"content", {{"type", "text"}, {"text", ""}}}});
        }

        // 将 Protobuf 二进制序列化为 JSON
        google::protobuf::DynamicMessageFactory response_factory;
        const google::protobuf::Message* response_prototype =
            response_factory.GetPrototype(tool->method->output_type());

        if (!response_prototype) {
            return MakeSuccess({
                {"content", {
                    {"type", "text"},
                    {"text", "Response received (binary data)"}
                }}
            });
        }

        std::unique_ptr<google::protobuf::Message> response_message(
            response_prototype->New());

        if (!response_message->ParseFromArray(response_bytes.data(),
                                               static_cast<int>(response_bytes.size()))) {
            return MakeError(kInternalError, "Failed to parse response",
                             params.value("id", nullptr));
        }

        google::protobuf::util::JsonPrintOptions json_options;
        json_options.preserve_proto_field_names = true;
        json_options.always_print_enums_as_ints = false;

        std::string response_json;
        auto status = google::protobuf::util::MessageToJsonString(*response_message,
                                                                   &response_json,
                                                                   json_options);

        if (!status.ok()) {
            return MakeError(kInternalError, "Failed to serialize response: " + status.ToString(),
                             params.value("id", nullptr));
        }

        nlohmann::json result_json = nlohmann::json::parse(response_json);

        nlohmann::json content = {
            {"type", "object"},
            {"data", result_json}
        };

        return MakeSuccess({{"content", content}});

    } catch (const std::exception& e) {
        return MakeError(kInternalError, std::string("RPC call failed: ") + e.what(),
                         params.value("id", nullptr));
    }
}

nlohmann::json MCPSession::HandleStreamingRpc(
    const ToolEntry* tool,
    const std::vector<uint8_t>& response_bytes,
    const nlohmann::json& id) {

    // 对于 server-streaming RPC，当前实现假设响应是单个聚合的消息
    // 在实际的流式场景中，KrpcChannel 应该返回多个消息
    // 这里我们演示聚合逻辑的框架

    // 边缘情况：空响应
    if (response_bytes.empty()) {
        nlohmann::json content_data = {
            {"type", "array"},
            {"data", nlohmann::json::array()}
        };
        return MakeSuccess({{"content", content_data}}, id);
    }

    // 检查大小限制
    if (response_bytes.size() > config_.aggregate_max_bytes) {
        return MakeError(kInternalError,
                         "Streaming response size (" + std::to_string(response_bytes.size()) +
                         " bytes) exceeds limit (" + std::to_string(config_.aggregate_max_bytes) +
                         " bytes)",
                         id);
    }

    // 获取响应类型
    google::protobuf::DynamicMessageFactory response_factory;
    const google::protobuf::Message* response_prototype =
        response_factory.GetPrototype(tool->method->output_type());

    if (!response_prototype) {
        // 无法获取原型时，返回提示
        nlohmann::json content_data = {
            {"type", "text"},
            {"text", "Streaming response received (binary data, type: " +
                     tool->method->output_type()->name() + ")"}
        };
        return MakeSuccess({{"content", content_data}}, id);
    }

    // 尝试解析响应消息
    // 注意：在实际的流式场景中，这里会有多个消息
    // 当前实现假设响应是单个消息，未来可以扩展为解析多个消息
    std::unique_ptr<google::protobuf::Message> response_message(
        response_prototype->New());

    if (!response_message->ParseFromArray(response_bytes.data(),
                                           static_cast<int>(response_bytes.size()))) {
        // 如果无法作为单个消息解析，可能是一个重复字段（repeated）
        // 这在 protobuf 中是常见的流式响应编码方式
        // 尝试作为 repeated 消息解析

        // 这里我们创建一个简化的 repeated 解析逻辑
        // 实际项目中可能需要更复杂的解析器来处理 protobuf _wire format_

        // 当前简化处理：假设整个响应是 repeated T 类型
        // 使用 aggregator 来处理和格式化
        std::vector<std::vector<uint8_t>> messages;
        messages.push_back(response_bytes);

        google::protobuf::util::JsonPrintOptions json_options;
        json_options.preserve_proto_field_names = true;

        auto result = aggregator_.AggregateMessages(
            [&]() {
                std::vector<std::unique_ptr<google::protobuf::Message>> msgs;
                auto* msg = response_prototype->New();
                if (msg->ParseFromArray(response_bytes.data(), response_bytes.size())) {
                    msgs.emplace_back(msg);
                } else {
                    delete msg;
                }
                return msgs;
            }(),
            json_options);

        if (!result.success) {
            return MakeError(kInternalError, "Failed to aggregate streaming response: " + result.error_message,
                             id);
        }

        // 解析聚合后的 JSON
        nlohmann::json aggregated_data;
        try {
            aggregated_data = nlohmann::json::parse(result.aggregated_json);
        } catch (const std::exception& e) {
            return MakeError(kInternalError, "Failed to parse aggregated JSON: " + std::string(e.what()),
                             id);
        }

        nlohmann::json content_data = {
            {"type", "array"},
            {"data", aggregated_data},
            {"streaming", true},
            {"messageCount", result.message_count}
        };
        return MakeSuccess({{"content", content_data}}, id);
    }

    // 成功解析单个消息 - 对于流式 RPC，这通常是第一个或唯一一个消息
    google::protobuf::util::JsonPrintOptions json_options;
    json_options.preserve_proto_field_names = true;

    std::string response_json;
    auto status = google::protobuf::util::MessageToJsonString(*response_message,
                                                               &response_json,
                                                               json_options);

    if (!status.ok()) {
        return MakeError(kInternalError, "Failed to serialize response: " + status.ToString(),
                         id);
    }

    nlohmann::json result_json = nlohmann::json::parse(response_json);

    // 构建 MCP 风格的响应，标记为流式
    nlohmann::json content_data = {
        {"type", "object"},
        {"data", result_json},
        {"streaming", true}
    };

    return MakeSuccess({{"content", content_data}}, id);
}

nlohmann::json MCPSession::MakeError(int code, const std::string& message,
                                      const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"error", {
            {"code", code},
            {"message", message}
        }},
        {"id", id}
    };
}

nlohmann::json MCPSession::MakeSuccess(const nlohmann::json& result,
                                        const nlohmann::json& id) {
    return {
        {"jsonrpc", "2.0"},
        {"result", result},
        {"id", id}
    };
}

} // namespace mcp_rpc
