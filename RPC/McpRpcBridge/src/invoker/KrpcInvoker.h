#ifndef MCP_RPC_BRIDGE_KRPC_INVOKER_H
#define MCP_RPC_BRIDGE_KRPC_INVOKER_H

#include "invoker/RpcInvoker.h"
#include <string>
#include <memory>
#include <unordered_map>

// KrpcChannel 是不完整类型（forward declaration）
// 具体实现需要链接 Krpc 框架
class KrpcChannel;

namespace mcp_rpc {

/**
 * @brief 基于现有 Krpc 框架的 RpcInvoker 实现
 */
class KrpcInvoker : public RpcInvoker {
public:
    KrpcInvoker();
    ~KrpcInvoker() override;

    /**
     * @brief 调用 RPC 方法
     */
    std::future<StatusOr<std::vector<uint8_t>>> Invoke(
        const std::string& method_full_name,
        const std::vector<uint8_t>& request_bytes) override;

    /**
     * @brief 设置是否立即连接
     */
    void SetConnectNow(bool connect_now) { connect_now_ = connect_now; }

private:
    bool connect_now_;

    // 缓存的 Channel，按方法全名索引
    // 当前未实现，需要链接 KrpcChannel
    // std::unordered_map<std::string, std::unique_ptr<KrpcChannel>> channels_;
};

/**
 * @brief KrpcInvoker 工厂函数
 */
std::shared_ptr<RpcInvoker> MakeKrpcInvoker(bool connect_now = false);

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_KRPC_INVOKER_H
