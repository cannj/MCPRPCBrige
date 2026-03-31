#ifndef MCP_RPC_BRIDGE_STREAMING_RESPONSE_AGGREGATOR_H
#define MCP_RPC_BRIDGE_STREAMING_RESPONSE_AGGREGATOR_H

#include "StreamingConfig.h"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>
#include <memory>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

namespace mcp_rpc {

/**
 * @brief 流式响应聚合结果
 */
struct AggregationResult {
    /**
     * @brief 是否成功聚合
     */
    bool success = false;

    /**
     * @brief 聚合后的 JSON 字符串
     *
     * 对于 server-streaming RPC，如果成功聚合则返回 JSON 数组；
     * 如果是 string 类型且使用 CONCAT 模式则返回连接的字符串。
     */
    std::string aggregated_json;

    /**
     * @brief 错误消息（如果失败）
     */
    std::string error_message;

    /**
     * @brief 总字节数（用于统计和限制）
     */
    size_t total_bytes = 0;

    /**
     * @brief 消息数量（用于统计）
     */
    int message_count = 0;
};

/**
 * @brief 流式响应聚合器
 *
 * 负责将 server-streaming RPC 的多个响应消息聚合成单个 JSON 响应。
 *
 * 支持的聚合模式：
 * - JSON_ARRAY: 将所有消息聚合成 JSON 数组
 * - CONCAT: 当响应是 string 类型时，连接所有字符串
 * - NONE: 不聚合，直接返回错误
 *
 * 边缘情况处理：
 * - 大小限制：超过 aggregate_max_bytes 返回错误
 * - 空响应：返回空数组或空字符串
 * - 单消息：直接返回该消息的 JSON 表示
 * - 错误传播：任何序列化失败都会返回错误
 */
class StreamingResponseAggregator {
public:
    /**
     * @brief 构造函数
     * @param config 流式处理配置
     */
    explicit StreamingResponseAggregator(const StreamingConfig& config);

    /**
     * @brief 聚合多个响应消息
     *
     * @param messages 响应消息列表（每个元素是已序列化的 Protobuf 消息字节）
     * @param response_type 响应消息的 Protobuf 类型描述符
     * @param to_json_fn 将单个消息序列化为 JSON 的函数
     * @return 聚合结果
     *
     * @note to_json_fn 的签名应为：std::string(const std::vector<uint8_t>& msg_bytes)
     *       返回单个消息的 JSON 字符串，失败时抛出异常或返回空字符串
     */
    AggregationResult Aggregate(
        const std::vector<std::vector<uint8_t>>& messages,
        const google::protobuf::Descriptor* response_type,
        std::function<std::string(const std::vector<uint8_t>&)> to_json_fn);

    /**
     * @brief 便捷方法：使用 Message 对象聚合
     *
     * @param messages 响应消息对象列表
     * @param json_options Protobuf JSON 序列化选项
     * @return 聚合结果
     */
    AggregationResult AggregateMessages(
        const std::vector<std::unique_ptr<google::protobuf::Message>>& messages,
        const google::protobuf::util::JsonPrintOptions& json_options = {});

    /**
     * @brief 检查是否超过大小限制
     * @param current_bytes 当前累计字节数
     * @param new_bytes 新增字节数
     * @return true 如果超过限制
     */
    bool WouldExceedLimit(size_t current_bytes, size_t new_bytes) const;

    /**
     * @brief 获取配置
     */
    const StreamingConfig& GetConfig() const { return config_; }

private:
    StreamingConfig config_;

    /**
     * @brief 将单个消息字节转换为 JSON 字符串
     */
    std::string MessageBytesToJson(
        const std::vector<uint8_t>& bytes,
        const google::protobuf::Descriptor* type,
        const google::protobuf::util::JsonPrintOptions& options);

    /**
     * @brief 检查响应类型是否是 string 类型
     */
    bool IsStringType(const google::protobuf::Descriptor* type) const;
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_STREAMING_RESPONSE_AGGREGATOR_H
