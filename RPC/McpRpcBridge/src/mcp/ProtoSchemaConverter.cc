#include "mcp/ProtoSchemaConverter.h"
#include <sstream>

namespace mcp_rpc {

void ProtoSchemaConverter::RegisterPostProcessor(std::string field_path,
                                                  SchemaPostProcessor processor) {
    post_processors_[std::move(field_path)] = std::move(processor);
}

nlohmann::json ProtoSchemaConverter::Convert(
    const google::protobuf::Descriptor* msg,
    nlohmann::json& defs) const {

    std::string full_name = msg->full_name();

    // 检查是否已存在（循环引用检测）
    if (defs.contains(full_name)) {
        return {{"$ref", "#/$defs/" + full_name}};
    }

    // 预留槽位，打破循环
    defs[full_name] = nullptr;

    nlohmann::json schema = {
        {"type", "object"},
        {"properties", nlohmann::json::object()}
    };

    // 处理字段
    for (int i = 0; i < msg->field_count(); ++i) {
        const auto* field = msg->field(i);
        std::string json_name = field->json_name();

        nlohmann::json field_schema;

        if (field->is_map()) {
            field_schema = ConvertMap(field, defs);
        } else if (field->is_repeated()) {
            field_schema = {
                {"type", "array"},
                {"items", ConvertField(field, defs)}
            };
        } else {
            field_schema = ConvertField(field, defs);
        }

        // 添加字段描述
        std::string comment = GetComment(field);
        if (!comment.empty()) {
            field_schema["description"] = comment;
        }

        schema["properties"][json_name] = field_schema;
    }

    // 处理 oneof
    for (int i = 0; i < msg->oneof_decl_count(); ++i) {
        const auto* oneof = msg->oneof_decl(i);
        nlohmann::json variants = nlohmann::json::array();

        for (int j = 0; j < oneof->field_count(); ++j) {
            const auto* field = oneof->field(j);
            variants.push_back(ConvertField(field, defs));
        }

        if (!variants.empty()) {
            schema["oneOf"].push_back(variants);
        }
    }

    // 更新槽位
    defs[full_name] = schema;

    return {{"$ref", "#/$defs/" + full_name}};
}

nlohmann::json ProtoSchemaConverter::ConvertField(
    const google::protobuf::FieldDescriptor* field,
    nlohmann::json& defs) const {

    nlohmann::json result = GetJsonType(field, defs);

    // 应用后处理器
    ApplyPostProcessor(field->full_name(), result);

    return result;
}

nlohmann::json ProtoSchemaConverter::ConvertMap(
    const google::protobuf::FieldDescriptor* field,
    nlohmann::json& defs) const {

    const auto* map_entry = field->message_type();
    const auto* key_field = map_entry->field(0);
    const auto* value_field = map_entry->field(1);

    // Key 类型约束 dropped（JSON object 的 key 总是 string）
    nlohmann::json result = {
        {"type", "object"},
        {"additionalProperties", ConvertField(value_field, defs)}
    };

    // 添加注释说明 key 类型
    if (key_field->type() != google::protobuf::FieldDescriptor::TYPE_STRING) {
        std::string desc = std::string("Note: map key type is ") +
                          key_field->type_name() +
                          ", but JSON requires string keys";
        result["description"] = desc;
    }

    return result;
}

nlohmann::json ProtoSchemaConverter::GetJsonType(
    const google::protobuf::FieldDescriptor* field,
    nlohmann::json& defs) const {

    using Type = google::protobuf::FieldDescriptor;

    switch (field->type()) {
        case Type::TYPE_STRING:
            return {{"type", "string"}};

        case Type::TYPE_BYTES:
            return {
                {"type", "string"},
                {"contentEncoding", "base64"}
            };

        case Type::TYPE_INT32:
        case Type::TYPE_INT64:
        case Type::TYPE_UINT32:
        case Type::TYPE_UINT64:
        case Type::TYPE_SINT32:
        case Type::TYPE_SINT64:
        case Type::TYPE_FIXED32:
        case Type::TYPE_FIXED64:
        case Type::TYPE_SFIXED32:
        case Type::TYPE_SFIXED64:
            return {{"type", "integer"}};

        case Type::TYPE_FLOAT:
        case Type::TYPE_DOUBLE:
            return {{"type", "number"}};

        case Type::TYPE_BOOL:
            return {{"type", "boolean"}};

        case Type::TYPE_ENUM: {
            nlohmann::json enum_values = nlohmann::json::array();
            const auto* enum_desc = field->enum_type();
            for (int i = 0; i < enum_desc->value_count(); ++i) {
                enum_values.push_back(enum_desc->value(i)->name());
            }
            return {
                {"type", "string"},
                {"enum", enum_values}
            };
        }

        case Type::TYPE_MESSAGE: {
            // 嵌套消息类型，递归转换
            return Convert(field->message_type(), defs);
        }

        case Type::TYPE_GROUP:
            // Group 已废弃，当作 object 处理
            return {{"type", "object"}};

        default:
            return {{"type", "object"}};
    }
}

std::string ProtoSchemaConverter::GetComment(
    const google::protobuf::FieldDescriptor* field) const {

    // TODO: 读取 proto 中的注释
    // 目前返回空字符串，后续可通过源文件信息解析

    // 检查是否有自定义选项
    // extend google.protobuf.FieldOptions {
    //     string description = 50001;
    // }

    return "";
}

void ProtoSchemaConverter::ApplyPostProcessor(
    std::string_view field_full_name,
    nlohmann::json& field_schema) const {

    auto it = post_processors_.find(std::string(field_full_name));
    if (it != post_processors_.end()) {
        it->second(field_full_name, field_schema);
    }
}

} // namespace mcp_rpc
