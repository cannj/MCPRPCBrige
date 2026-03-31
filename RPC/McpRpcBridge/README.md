# MCP-RPC Bridge

将 Protobuf 定义的 RPC 服务自动暴露为 MCP (Model Context Protocol) 工具。

## 概述

MCP-RPC Bridge 是一个轻量级适配层，允许 LLM 通过 MCP 协议直接调用后端的 RPC 服务，无需为每个服务编写集成代码。

### 特性

- **零样板代码** - 在服务级别注册一次，所有方法自动暴露
- **高性能** - 懒加载 Schema 生成，零拷贝反序列化
- **完全解耦** - MCP 层与传输层通过单一接口隔离
- **小体积** - 核心代码约 1000 行
- **LLM 友好** - 优雅处理未知字段、长工具名

## 目录结构

```
McpRpcBridge/
├── src/
│   ├── mcp/                    # MCP 适配层
│   │   ├── LazyValue.h         # 线程安全延迟初始化
│   │   ├── ToolRegistry.h/cc   # 工具注册表
│   │   ├── ProtoSchemaConverter.h/cc  # Protobuf → JSON Schema
│   │   ├── ArgumentsDeserializer.h/cc # JSON → Protobuf
│   │   ├── MCPSession.h/cc     # JSON-RPC 会话管理
│   │   └── StreamingConfig.h   # 流式 RPC 配置
│   ├── invoker/                # RPC 调用器层
│   │   ├── RpcInvoker.h        # 抽象接口（跨层边界）
│   │   └── KrpcInvoker.h/cc    # 基于 Krpc 的实现
│   └── McpRpcBridge.h          # 总入口头文件
├── example/
│   ├── server/                 # 示例服务器
│   └── client/                 # 示例客户端
├── test/                       # 单元测试
└── CMakeLists.txt
```

## 快速开始

### 依赖

- C++17 或更高版本
- CMake 3.14+
- Protobuf
- nlohmann/json

### 构建

```bash
cd McpRpcBridge
mkdir build && cd build
cmake ..
make -j4
```

### 基本使用

```cpp
#include "McpRpcBridge.h"

int main() {
    // 1. 创建工具注册表
    mcp_rpc::ToolRegistry registry;

    // 2. 注册 RPC 服务（自动注册所有方法）
    auto* service = new MyRpcService();
    const auto* desc = service->GetDescriptor();
    registry.RegisterService(desc);

    // 3. 创建 RPC Invoker
    auto invoker = mcp_rpc::MakeKrpcInvoker();

    // 4. 创建 MCP Session
    mcp_rpc::MCPSession session(registry, invoker);

    // 5. 处理 MCP 请求
    std::string request = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "tools/list"
    })";

    auto response = session.HandleRequest(nlohmann::json::parse(request));
    std::cout << response.dump(2) << std::endl;

    return 0;
}
```

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      LLM / MCP Client                        │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ JSON-RPC 2.0
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      MCP Adapter Layer                       │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │ MCPSession  │  │ ToolRegistry │  │ ProtoSchemaConverter│ │
│  │ (协议处理)  │  │ (工具注册)    │  │ (Schema 生成)        │ │
│  └─────────────┘  └──────────────┘  └─────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │           ArgumentsDeserializer (JSON → Proto)          │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ RpcInvoker 接口
                              │ (serialized bytes in/out)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Transport Layer                         │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │KrpcInvoker  │  │GrpcInvoker   │  │ InProcInvoker       │ │
│  │(Krpc 框架)   │  │(gRPC)        │  │ (进程内调用)         │ │
│  └─────────────┘  └──────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Backend RPC Services                       │
└─────────────────────────────────────────────────────────────┘
```

## 类型映射

| Protobuf 类型 | JSON Schema |
|-------------|-------------|
| string | {"type": "string"} |
| bytes | {"type": "string", "contentEncoding": "base64"} |
| int32/64, uint32/64 | {"type": "integer"} |
| float/double | {"type": "number"} |
| bool | {"type": "boolean"} |
| enum | {"type": "string", "enum": [...]} |
| message | {"$ref": "#/$defs/TypeName"} |
| repeated | {"type": "array", "items": {...}} |
| map<K,V> | {"type": "object", "additionalProperties": {...}} |
| oneof | {"oneOf": [...]} |

## 配置选项

### StreamingConfig

```cpp
mcp_rpc::StreamingConfig config;
config.aggregate_max_bytes = 1 * 1024 * 1024;  // 流式响应聚合阈值
config.register_client_streaming = false;       // 是否注册 client-streaming
config.register_bidi_streaming = false;         // 是否注册 bidi-streaming
```

### 自定义 FieldOptions

在 proto 文件中定义自定义选项：

```protobuf
extend google.protobuf.FieldOptions {
    string description = 50001;  // 覆盖自动生成的字段描述
    string example     = 50002;  // JSON 示例值
    bool   required    = 50003;  // 标记为必填
    string format      = 50004;  // JSON Schema format
}
```

## 测试

运行单元测试：

```bash
cd build
ctest --output-on-failure
```

## 待实现功能

当前框架已搭建，以下组件需要进一步完善：

- [ ] `ProtoSchemaConverter::GetComment` - 读取 proto 注释
- [ ] `KrpcInvoker::Invoke` - 完整的 KrpcChannel 适配
- [ ] 流式 RPC 响应聚合逻辑
- [ ] MCPSession 响应序列化（Protobuf → JSON）
- [ ] 自定义 FieldOptions 支持

## License

MIT License
