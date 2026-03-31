#ifndef MCP_RPC_BRIDGE_H
#define MCP_RPC_BRIDGE_H

/**
 * @file McpRpcBridge.h
 * @brief MCP-RPC Bridge 总入口头文件
 *
 * 包含所有公共 API 的便捷包含文件。
 */

// MCP 层
#include "mcp/LazyValue.h"
#include "mcp/ToolRegistry.h"
#include "mcp/ProtoSchemaConverter.h"
#include "mcp/ArgumentsDeserializer.h"
#include "mcp/MCPSession.h"
#include "mcp/StreamingConfig.h"

// Invoker 层
#include "invoker/RpcInvoker.h"
#include "invoker/KrpcInvoker.h"

namespace mcp_rpc {

/**
 * @brief 库版本号
 */
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

/**
 * @brief 获取版本字符串
 */
inline const char* GetVersionString() {
    return "1.0.0";
}

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_H
