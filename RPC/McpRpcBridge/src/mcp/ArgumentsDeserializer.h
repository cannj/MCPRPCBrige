#ifndef MCP_RPC_BRIDGE_ARGUMENTS_DESERIALIZER_H
#define MCP_RPC_BRIDGE_ARGUMENTS_DESERIALIZER_H

#include <string>
#include <vector>
#include <span>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

namespace mcp_rpc {

/**
 * @brief JSON 参数反序列化器
 *
 * 负责将 MCP 工具调用中的 JSON 参数转换为 Protobuf 二进制格式。
 *
 * 特性：
 * - 使用 protobuf 官方 JsonStringToMessage 进行转换
 * - 支持未知字段容错（LLM 可能传入额外参数）
 * - 零拷贝路径（当输入为 string_view 时）
 */
class ArgumentsDeserializer {
public:
    ArgumentsDeserializer();
    ~ArgumentsDeserializer() = default;

    /**
     * @brief 配置未知字段处理行为
     * @param ignore_unknown true=忽略未知字段，false=报错
     */
    void SetIgnoreUnknownFields(bool ignore_unknown);

    /**
     * @brief 将 JSON 参数反序列化为 Protobuf 消息
     *
     * @param json_input JSON 输入数据
     * @param message 目标 Protobuf 消息对象（必须已创建）
     * @return 错误信息，空表示成功
     *
     * @note 调用者负责创建和销毁 message 对象
     */
    std::string Deserialize(std::string_view json_input,
                            google::protobuf::Message* message) const;

    /**
     * @brief 将 JSON 参数直接反序列化为二进制 Protobuf 数据
     *
     * @param json_input JSON 输入数据
     * @param message 目标 Protobuf 消息的原型（用于创建新实例）
     * @param output 输出二进制数据
     * @return 错误信息，空表示成功
     */
    std::string DeserializeToBytes(std::string_view json_input,
                                    const google::protobuf::Message& prototype,
                                    std::vector<uint8_t>& output) const;

private:
    google::protobuf::util::JsonParseOptions parse_options_;
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_ARGUMENTS_DESERIALIZER_H
