#ifndef MCP_RPC_BRIDGE_KRPC_INVOKER_H
#define MCP_RPC_BRIDGE_KRPC_INVOKER_H

#include "invoker/RpcInvoker.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <google/protobuf/service.h>

// KrpcChannel 前向声明
class KrpcChannel;

// KrpcController 前向声明
class Krpccontroller;

namespace mcp_rpc {

/**
 * @brief 基于现有 Krpc 框架的 RpcInvoker 实现
 *
 * KrpcInvoker 是 MCP 层与 Krpc 框架之间的桥梁，它：
 * 1. 解析方法全名（如 "/package.Service/Method"）
 * 2. 管理 KrpcChannel 实例（按服务复用连接）
 * 3. 将 Protobuf 消息序列化并通过 Channel 发送
 * 4. 将响应反序列化为字节向量返回
 */
class KrpcInvoker : public RpcInvoker {
public:
    KrpcInvoker();
    ~KrpcInvoker() override;

    /**
     * @brief 调用 RPC 方法
     * @param method_full_name 方法全名（如 "/package.Service/Method"）
     * @param request_bytes 序列化的请求数据
     * @return 包含响应数据的 future
     */
    std::future<StatusOr<std::vector<uint8_t>>> Invoke(
        const std::string& method_full_name,
        const std::vector<uint8_t>& request_bytes) override;

    /**
     * @brief 设置是否立即连接
     */
    void SetConnectNow(bool connect_now) { connect_now_ = connect_now; }

private:
    /**
     * @brief 解析方法全名
     * @param full_name 方法全名（如 "/package.Service/Method" 或 "Service/Method"）
     * @param package 输出参数：包名
     * @param service 输出参数：服务名
     * @param method 输出参数：方法名
     * @return 解析是否成功
     */
    bool ParseMethodName(const std::string& full_name,
                          std::string& package,
                          std::string& service,
                          std::string& method);

    /**
     * @brief 获取或创建 Channel
     * @param service_name 服务名
     * @return Channel 指针，失败返回 nullptr
     */
    KrpcChannel* GetOrCreateChannel(const std::string& service_name);

    bool connect_now_;

    // 缓存的 Channel，按服务名索引
    std::unordered_map<std::string, std::unique_ptr<KrpcChannel>> channels_;

    // 线程安全
    std::mutex channels_mutex_;
};

/**
 * @brief KrpcInvoker 工厂函数
 */
std::shared_ptr<RpcInvoker> MakeKrpcInvoker(bool connect_now = false);

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_KRPC_INVOKER_H
