#ifndef MCP_RPC_BRIDGE_PROTO_SCHEMA_CONVERTER_H
#define MCP_RPC_BRIDGE_PROTO_SCHEMA_CONVERTER_H

#include <nlohmann/json.hpp>
#include <google/protobuf/descriptor.h>
#include <functional>

namespace mcp_rpc {

/**
 * @brief 后处理回调，用于自定义字段 Schema
 *
 * 允许用户在生成后的 Schema 上添加额外的约束条件，
 * 如最小值、最大值、正则表达式等。
 */
using SchemaPostProcessor =
    std::function<void(std::string_view field_full_name, nlohmann::json& field_schema)>;

/**
 * @brief Protobuf 描述符到 JSON Schema 的转换器
 *
 * 无状态工具类，提供纯函数式的转换方法。
 * 支持：
 * - 所有基本类型映射
 * - 嵌套消息类型（$defs + $ref）
 * - repeated 字段（array）
 * - map 字段（object）
 * - oneof 字段（oneOf）
 * - enum 字段（string + enum）
 * - 循环引用检测
 */
class ProtoSchemaConverter {
public:
    ProtoSchemaConverter() = default;

    /**
     * @brief 注册 Schema 后处理器
     * @param field_path 字段全路径（如 "package.Message.field"）
     * @param processor 处理函数
     */
    void RegisterPostProcessor(std::string field_path, SchemaPostProcessor processor);

    /**
     * @brief 将 Protobuf 消息类型转换为 JSON Schema
     *
     * @param msg 消息描述符
     * @param defs 输出参数，存放 $defs 中的子 Schema
     * @return JSON Schema 对象
     *
     * @note defs 参数用于打破循环引用，同一消息类型在不同位置
     *       引用时会复用 $defs 中的定义。
     */
    nlohmann::json Convert(const google::protobuf::Descriptor* msg,
                           nlohmann::json& defs) const;

private:
    std::unordered_map<std::string, SchemaPostProcessor> post_processors_;

    /**
     * @brief 转换单个字段
     */
    nlohmann::json ConvertField(const google::protobuf::FieldDescriptor* field,
                                 nlohmann::json& defs) const;

    /**
     * @brief 转换 map 字段
     */
    nlohmann::json ConvertMap(const google::protobuf::FieldDescriptor* field,
                               nlohmann::json& defs) const;

    /**
     * @brief 获取字段的 JSON 类型
     */
    nlohmann::json GetJsonType(const google::protobuf::FieldDescriptor* field,
                                nlohmann::json& defs) const;

    /**
     * @brief 获取字段的描述（来自 proto 注释或自定义选项）
     */
    std::string GetComment(const google::protobuf::FieldDescriptor* field) const;

    /**
     * @brief 应用后处理器（如果有）
     */
    void ApplyPostProcessor(std::string_view field_full_name,
                            nlohmann::json& field_schema) const;
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_PROTO_SCHEMA_CONVERTER_H
