#include "mcp/ToolRegistry.h"
#include "mcp/ProtoSchemaConverter.h"
#include <sstream>

namespace mcp_rpc {

// ToolEntry::GetInputSchema 实现
const nlohmann::json& ToolEntry::GetInputSchema() const {
    return schema.GetOrInit([this]() {
        nlohmann::json defs;
        ProtoSchemaConverter converter;
        auto s = converter.Convert(method->input_type(), defs);
        s["$defs"] = std::move(defs);
        return s;
    });
}

// ToolRegistry 实现
void ToolRegistry::RegisterService(const google::protobuf::ServiceDescriptor* service) {
    std::string service_name = service->name();
    int method_count = service->method_count();

    for (int i = 0; i < method_count; ++i) {
        const google::protobuf::MethodDescriptor* method = service->method(i);

        // 生成工具名称和描述
        std::string name = MakeToolName(method);
        std::string description = "[RPC] " + method->full_name() +
                                  " — input: " + method->input_type()->name() +
                                  ", output: " + method->output_type()->name();

        // 检查是否为流式 RPC，添加标记
        if (method->client_streaming() || method->server_streaming()) {
            description += " [streaming]";
        }

        // 使用 make_unique 创建 ToolEntry（需要访问私有构造函数）
        auto entry = std::unique_ptr<ToolEntry>(new ToolEntry(
            std::move(name),
            std::move(description),
            method
        ));

        tools_[entry->name] = std::move(entry);
    }
}

const ToolEntry* ToolRegistry::FindTool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<const ToolEntry*> ToolRegistry::ListTools() const {
    std::vector<const ToolEntry*> result;
    result.reserve(tools_.size());
    for (const auto& kv : tools_) {
        result.push_back(kv.second.get());
    }
    return result;
}

std::string ToolRegistry::MakeToolName(const google::protobuf::MethodDescriptor* method) {
    std::string full = method->service()->name() + "_" + method->name();

    // 超过 48 字符时使用截断 + 哈希
    if (full.size() <= 48) {
        return full;
    }

    // 简单哈希实现（实际可使用 Murmur3）
    auto hash = std::hash<std::string>{}(full);
    std::stringstream ss;
    ss << std::hex << hash;
    std::string hash_str = ss.str();

    return full.substr(0, 32) + "_" + hash_str.substr(0, 8);
}

} // namespace mcp_rpc
