#include "mcp/ArgumentsDeserializer.h"

namespace mcp_rpc {

ArgumentsDeserializer::ArgumentsDeserializer() {
    // 默认配置：忽略未知字段
    parse_options_.ignore_unknown_fields = true;
}

void ArgumentsDeserializer::SetIgnoreUnknownFields(bool ignore_unknown) {
    parse_options_.ignore_unknown_fields = ignore_unknown;
}

std::string ArgumentsDeserializer::Deserialize(
    std::string_view json_input,
    google::protobuf::Message* message) const {

    std::string json_str(json_input);
    auto status = google::protobuf::util::JsonStringToMessage(json_str, message, parse_options_);

    if (!status.ok()) {
        return status.message().ToString();
    }

    return "";  // 成功
}

std::string ArgumentsDeserializer::DeserializeToBytes(
    std::string_view json_input,
    const google::protobuf::Message& prototype,
    std::vector<uint8_t>& output) const {

    // 创建临时消息对象
    std::unique_ptr<google::protobuf::Message> message(prototype.New());

    // 反序列化 JSON 到消息
    std::string error = Deserialize(json_input, message.get());
    if (!error.empty()) {
        return error;
    }

    // 序列化为二进制
    size_t size = message->ByteSizeLong();
    output.resize(size);

    if (!message->SerializeToArray(output.data(), static_cast<int>(size))) {
        return "Failed to serialize message to binary";
    }

    return "";  // 成功
}

} // namespace mcp_rpc
