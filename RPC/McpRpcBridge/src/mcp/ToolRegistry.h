#ifndef MCP_RPC_BRIDGE_TOOL_REGISTRY_H
#define MCP_RPC_BRIDGE_TOOL_REGISTRY_H

#include "LazyValue.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <google/protobuf/descriptor.h>
#include <nlohmann/json.hpp>

namespace mcp_rpc {

/**
 * @brief 工具条目，包含 MCP 工具的元数据
 *
 * 每个 ToolEntry 对应一个 RPC 方法，包含工具名称、描述、
 * 方法描述符和懒加载的 JSON Schema。
 *
 * 设计原则：
 * 1. 所有字段均为 const，确保不可变性（除了懒加载的 schema）
 * 2. 使用私有构造函数 + 工厂函数，确保对象完整性
 * 3. 删除拷贝和移动操作，防止意外转移所有权
 */
struct ToolEntry {
    const std::string name;                        // 工具唯一名称
    const std::string description;                 // 工具描述
    const google::protobuf::MethodDescriptor* method;  // Protobuf 方法描述符
    mutable LazyValue<nlohmann::json> schema;      // 懒加载的输入 Schema

    /**
     * @brief 获取输入消息的 JSON Schema
     *
     * 首次调用时生成并缓存 Schema，后续调用直接返回缓存值。
     */
    const nlohmann::json& GetInputSchema() const;

private:
    // 私有构造函数，通过 ToolRegistry 创建
    ToolEntry(std::string name,
              std::string description,
              const google::protobuf::MethodDescriptor* method)
        : name(std::move(name))
        , description(std::move(description))
        , method(method)
        , schema() {}

    // 声明为友元，允许 ToolRegistry 创建对象
    friend class ToolRegistry;

    // 删除拷贝和移动操作（const 成员无法被移动赋值）
    ToolEntry(const ToolEntry&) = delete;
    ToolEntry& operator=(const ToolEntry&) = delete;
    ToolEntry(ToolEntry&&) = delete;
    ToolEntry& operator=(ToolEntry&&) = delete;
};

/**
 * @brief 工具注册表，管理所有已注册的 MCP 工具
 *
 * 启动时一次性构建，之后只读访问，无锁高效查询。
 *
 * 设计原则：
 * 1. 使用 unique_ptr 存储 ToolEntry，避免 map rehash 时的移动问题
 * 2. 查找操作返回 const 指针，确保只读访问
 * 3. 注册完成后成为只读状态
 */
class ToolRegistry {
public:
    ToolRegistry() = default;

    /**
     * @brief 注册一个服务的所有方法
     * @param service Protobuf 服务描述符
     * @param name_prefix 服务名前缀（用于生成工具名称）
     */
    void RegisterService(const google::protobuf::ServiceDescriptor* service);

    /**
     * @brief 根据工具名称查找工具条目
     * @param name 工具名称
     * @return ToolEntry 指针，未找到返回 nullptr
     */
    const ToolEntry* FindTool(const std::string& name) const;

    /**
     * @brief 获取所有已注册的工具列表
     * @return 工具条目列表（用于 tools/list 响应）
     */
    std::vector<const ToolEntry*> ListTools() const;

    /**
     * @brief 检查注册表是否为空
     */
    bool Empty() const { return tools_.empty(); }

    /**
     * @brief 获取工具数量
     */
    size_t Size() const { return tools_.size(); }

private:
    // 工具名称 -> ToolEntry (unique_ptr 避免移动)
    std::unordered_map<std::string, std::unique_ptr<ToolEntry>> tools_;

    /**
     * @brief 生成工具名称
     *
     * 格式：ServiceName_MethodName
     * 超过 48 字符时使用截断 + 哈希
     */
    static std::string MakeToolName(const google::protobuf::MethodDescriptor* method);
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_TOOL_REGISTRY_H
