#ifndef MCP_RPC_BRIDGE_MCP_SESSION_H
#define MCP_RPC_BRIDGE_MCP_SESSION_H

#include "ToolRegistry.h"
#include "ArgumentsDeserializer.h"
#include "StreamingConfig.h"
#include "invoker/RpcInvoker.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace mcp_rpc {

/**
 * @brief MCP 会话状态
 *
 * 实现 JSON-RPC 2.0 状态机：
 * initialize -> tools/list -> tools/call
 */
enum class SessionState {
    New,           // 新建，等待初始化
    Initialized,   // 已初始化
    Closed         // 已关闭
};

/**
 * @brief MCP 会话管理器
 *
 * 负责处理 MCP 协议的核心逻辑：
 * - JSON-RPC 2.0 请求解析和响应构建
 * - tools/list：返回所有可用工具及其 Schema
 * - tools/call：执行工具调用并返回结果
 *
 * 无状态设计，每个请求独立处理。
 */
class MCPSession {
public:
    /**
     * @brief 构造函数
     *
     * @param registry 工具注册表（必须已初始化）
     * @param invoker RPC 调用器（必须非空）
     * @param config 流式处理配置（可选，使用默认值）
     */
    explicit MCPSession(const ToolRegistry& registry,
                        std::shared_ptr<RpcInvoker> invoker,
                        StreamingConfig config = {});
    ~MCPSession() = default;

    /**
     * @brief 处理 MCP 请求
     *
     * @param request JSON-RPC 请求
     * @return JSON-RPC 响应
     *
     * 支持的 method：
     * - initialize: 初始化会话
     * - tools/list: 列出所有工具
     * - tools/call: 调用工具
     */
    nlohmann::json HandleRequest(const nlohmann::json& request);

    /**
     * @brief 获取当前会话状态
     */
    SessionState GetState() const { return state_; }

private:
    const ToolRegistry& registry_;
    std::shared_ptr<RpcInvoker> invoker_;
    StreamingConfig config_;
    SessionState state_;
    ArgumentsDeserializer deserializer_;

    // 协议处理方法
    nlohmann::json HandleInitialize(const nlohmann::json& params, const nlohmann::json& id, bool is_notification);
    nlohmann::json HandleToolsList(const nlohmann::json& params, const nlohmann::json& id, bool is_notification);
    nlohmann::json HandleToolsCall(const nlohmann::json& params, const nlohmann::json& id, bool is_notification);

    // 错误处理
    static nlohmann::json MakeError(int code, const std::string& message,
                                     const nlohmann::json& id = nullptr);
    static nlohmann::json MakeSuccess(const nlohmann::json& result,
                                       const nlohmann::json& id = nullptr);

    // JSON-RPC 错误码
    static constexpr int kParseError = -32700;
    static constexpr int kInvalidRequest = -32600;
    static constexpr int kMethodNotFound = -32601;
    static constexpr int kInvalidParams = -32602;
    static constexpr int kInternalError = -32603;
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_MCP_SESSION_H
