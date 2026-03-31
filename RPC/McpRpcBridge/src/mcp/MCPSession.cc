#include "mcp/MCPSession.h"
#include <iostream>

namespace mcp_rpc {

MCPSession::MCPSession(const ToolRegistry& registry,
                       std::shared_ptr<RpcInvoker> invoker,
                       StreamingConfig config)
    : registry_(registry)
    , invoker_(std::move(invoker))
    , config_(std::move(config))
    , state_(SessionState::New)
    , deserializer_() {}

nlohmann::json MCPSession::HandleRequest(const nlohmann::json& request) {
    try {
        // 解析 JSON-RPC 请求
        std::string method = request.value("method", "");
        nlohmann::json params = request.value("params", nlohmann::json::object());
        nlohmann::json id = request.value("id", nullptr);

        // 路由到对应的处理方法
        if (method == "initialize") {
            return HandleInitialize(params);
        } else if (method == "tools/list") {
            if (state_ != SessionState::Initialized) {
                return MakeError(kInvalidRequest, "Session not initialized. Call 'initialize' first.", id);
            }
            return HandleToolsList(params);
        } else if (method == "tools/call") {
            if (state_ != SessionState::Initialized) {
                return MakeError(kInvalidRequest, "Session not initialized. Call 'initialize' first.", id);
            }
            return HandleToolsCall(params);
        } else {
            return MakeError(kMethodNotFound, "Unknown method: " + method, id);
        }

    } catch (const std::exception& e) {
        return MakeError(kInternalError, std::string("Internal error: ") + e.what(),
                         request.value("id", nullptr));
    }
}

nlohmann::json MCPSession::HandleInitialize(const nlohmann::json& params) {
    state_ = SessionState::Initialized;

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

    return MakeSuccess(result);
}

nlohmann::json MCPSession::HandleToolsList(const nlohmann::json& params) {
    nlohmann::json tools = nlohmann::json::array();

    for (const auto* entry : registry_.ListTools()) {
        nlohmann::json tool = {
            {"name", entry->name},
            {"description", entry->description},
            {"inputSchema", entry->GetInputSchema()}
        };
        tools.push_back(tool);
    }

    return MakeSuccess({{"tools", tools}});
}

nlohmann::json MCPSession::HandleToolsCall(const nlohmann::json& params) {
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
        return MakeError(kMethodNotFound,
                         "Unknown tool: " + tool_name + ". Available: " +
                         std::string(available.empty() ? "none" : available[0].c_str()),
                         params.value("_meta", nlohmann::json(nullptr)));
    }

    // 将参数序列化为 JSON 字符串
    std::string args_json = arguments.dump();

    // 反序列化为 Protobuf 二进制
    std::vector<uint8_t> request_bytes;
    // 使用 MessageFactory 创建消息原型
    const google::protobuf::Message* prototype =
        google::protobuf::MessageFactory::generated_factory()
            ->GetPrototype(tool->method->input_type());
    if (!prototype) {
        return MakeError(kInternalError, "Failed to get message prototype",
                         params.value("_meta", nlohmann::json(nullptr)));
    }
    std::string error = deserializer_.DeserializeToBytes(
        args_json,
        *prototype,
        request_bytes);

    if (!error.empty()) {
        return MakeError(kInvalidParams, "Failed to parse arguments: " + error,
                         params.value("_meta", nlohmann::json(nullptr)));
    }

    // 调用 RPC
    // TODO: 处理流式 RPC
    auto future = invoker_->Invoke(tool->method->full_name(),
                                    request_bytes);

    // 等待响应（同步）
    // TODO: 支持异步返回
    try {
        auto response_status = future.get();
        // TODO: 将 response_status 转换为 JSON

        // 临时实现：返回空结果
        return MakeSuccess({{"content", {{"type", "text"}, {"text", "OK"}}}});

    } catch (const std::exception& e) {
        return MakeError(kInternalError, std::string("RPC call failed: ") + e.what(),
                         params.value("_meta", nlohmann::json(nullptr)));
    }
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
