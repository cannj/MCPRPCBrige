#ifndef MCP_RPC_BRIDGE_MOCK_RPC_INVOKER_H
#define MCP_RPC_BRIDGE_MOCK_RPC_INVOKER_H

#include "invoker/RpcInvoker.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <future>

namespace mcp_rpc {

/**
 * @brief RpcInvoker 的模拟实现，用于端到端测试
 *
 * 设计原则：
 * 1. 可配置的响应行为（成功/失败/延迟）
 * 2. 记录所有调用以便验证
 * 3. 支持方法级别的响应配置
 * 4. 线程安全
 */
class MockRpcInvoker : public RpcInvoker {
public:
    MockRpcInvoker() = default;
    ~MockRpcInvoker() override = default;

    /**
     * @brief 配置特定方法的响应
     * @param method_full_name 方法全名（如 "/package.Service/Method"）
     * @param response_bytes 返回的响应数据
     */
    void SetResponse(const std::string& method_full_name,
                     const std::vector<uint8_t>& response_bytes);

    /**
     * @brief 配置特定方法的错误响应
     * @param method_full_name 方法全名
     * @param error_code 错误码
     * @param error_message 错误消息
     */
    void SetErrorResponse(const std::string& method_full_name,
                          StatusCode error_code,
                          const std::string& error_message);

    /**
     * @brief 配置默认响应（未明确配置的方法使用此响应）
     * @param response_bytes 默认返回的响应数据
     */
    void SetDefaultResponse(const std::vector<uint8_t>& response_bytes);

    /**
     * @brief 配置默认错误响应
     * @param error_code 错误码
     * @param error_message 错误消息
     */
    void SetDefaultErrorResponse(StatusCode error_code,
                                  const std::string& error_message);

    /**
     * @brief 设置模拟延迟（毫秒）
     * @param delay_ms 延迟时间，0 表示无延迟
     */
    void SetSimulatedDelay(int delay_ms) { delay_ms_ = delay_ms; }

    /**
     * @brief 调用 RPC 方法（模拟实现）
     */
    std::future<StatusOr<std::vector<uint8_t>>> Invoke(
#if __cplusplus >= 202002L
        std::string_view method_full_name,
        std::span<const uint8_t> request_bytes
#else
        const std::string& method_full_name,
        const std::vector<uint8_t>& request_bytes
#endif
    ) override;

    /**
     * @brief 获取调用历史记录
     * @return 调用历史列表，每项为 {method_name, request_bytes}
     */
    std::vector<std::pair<std::string, std::vector<uint8_t>>> GetCallHistory() const;

    /**
     * @brief 获取特定方法的调用次数
     * @param method_full_name 方法全名
     * @return 调用次数
     */
    int GetCallCount(const std::string& method_full_name) const;

    /**
     * @brief 获取总调用次数
     * @return 总调用次数
     */
    int GetTotalCallCount() const;

    /**
     * @brief 清除所有历史记录
     */
    void ClearHistory();

    /**
     * @brief 清除所有配置（响应和错误配置）
     */
    void ClearConfig();

private:
    struct ResponseConfig {
        bool is_error = false;
        std::vector<uint8_t> response_data;
        StatusCode error_code = StatusCode::OK;
        std::string error_message;
    };

    // 方法级别的响应配置
    std::unordered_map<std::string, ResponseConfig> method_responses_;

    // 默认响应配置
    ResponseConfig default_response_;

    // 模拟延迟（毫秒）
    int delay_ms_ = 0;

    // 调用历史（线程安全）
    mutable std::mutex history_mutex_;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> call_history_;

    // 调用计数（线程安全）
    mutable std::mutex count_mutex_;
    std::unordered_map<std::string, int> call_counts_;
    int total_calls_ = 0;
};

/**
 * @brief 创建 MockRpcInvoker 的工厂函数
 */
std::shared_ptr<MockRpcInvoker> MakeMockRpcInvoker();

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_MOCK_RPC_INVOKER_H
