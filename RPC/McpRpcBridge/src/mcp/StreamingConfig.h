#ifndef MCP_RPC_BRIDGE_STREAMING_CONFIG_H
#define MCP_RPC_BRIDGE_STREAMING_CONFIG_H

#include <cstddef>
#include <string>

namespace mcp_rpc {

/**
 * @brief 流式 RPC 处理配置
 *
 * MCP 是请求 - 响应模式，需要明确定义如何处理流式 RPC。
 */
struct StreamingConfig {
    /**
     * @brief 聚合响应最大字节数
     *
     * 对于 server-streaming RPC，如果响应较小则聚合成 JSON 数组返回；
     * 如果超过此阈值则返回错误，建议客户端使用专用订阅工具。
     */
    size_t aggregate_max_bytes = 1 * 1024 * 1024;  // 1 MB 默认

    /**
     * @brief 是否注册 client-streaming RPC
     *
     * 默认为 false，因为 MCP 不支持客户端流式调用。
     */
    bool register_client_streaming = false;

    /**
     * @brief 是否注册 bidi-streaming RPC
     *
     * 默认为 false，因为 MCP 不支持双向流式调用。
     */
    bool register_bidi_streaming = false;

    /**
     * @brief 聚合模式说明
     *
     * 当聚合启用且响应在阈值内时，streaming RPC 的响应会被
     * 序列化为 JSON 数组返回给 LLM。
     */
    enum class AggregateMode {
        NONE,       // 不聚合，直接返回错误
        JSON_ARRAY, // 聚合成 JSON 数组
        CONCAT      // 聚合成连接字符串（仅当响应是 string 类型）
    };
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_STREAMING_CONFIG_H
