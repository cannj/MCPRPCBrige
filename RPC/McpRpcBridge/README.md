# MCP-RPC Bridge

将 Protobuf 定义的 RPC 服务自动暴露为 MCP (Model Context Protocol) 工具。

## 概述

MCP-RPC Bridge 是一个轻量级适配层，允许 LLM 通过 MCP 协议直接调用后端的 RPC 服务，无需为每个服务编写集成代码。

### 核心特性

- **零样板代码** - 在服务级别注册一次，所有方法自动暴露为 MCP 工具
- **高性能** - 懒加载 Schema 生成，零拷贝反序列化
- **完全解耦** - MCP 层与传输层通过 `RpcInvoker` 接口隔离
- **LLM 友好** - 自动 Protobuf ↔ JSON Schema 转换，优雅处理未知字段
- **流式支持** - 完整的 server-streaming RPC 聚合处理
- **小体积** - 核心库约 2,400 行代码

## 状态

✅ **v1.0.0 已发布 - 所有核心功能已完成并经过完整测试**

### 已完成功能

- [x] 工具注册表（ToolRegistry）- 服务注册与发现
- [x] Protobuf ↔ JSON Schema 双向转换
- [x] JSON-RPC 2.0 协议完整实现
- [x] MCP 核心方法：`initialize`, `tools/list`, `tools/call`
- [x] Unary RPC 调用支持
- [x] Server-Streaming RPC 调用与响应聚合
- [x] 流式响应聚合器（JSON 数组/字符串连接模式）
- [x] KrpcInvoker 完整实现
- [x] MockRpcInvoker 用于测试
- [x] 完整的单元测试套件（12 个测试，100% 通过）
- [x] 端到端集成测试

## 快速开始

### 依赖

- C++17 或更高版本
- CMake 3.14+
- Protobuf
- nlohmann/json
- glog
- ZooKeeper (Krpc 依赖)
- muduo (Krpc 依赖)

### 构建

```bash
cd McpRpcBridge
mkdir build && cd build
cmake .. -DMCP_RPC_BUILD_EXAMPLES=ON  # 添加示例程序
make -j1  # 使用单线程编译避免内存问题
ctest --output-on-failure  # 运行测试
```

### 运行 MCP 服务器

启动服务器，让 LLM 通过 MCP 协议发现和调用 RPC 函数：

```bash
cd build
./example/server/mcp_example_server -i ../bin/rpc.conf
```

服务器启动后会监听端口 9090，显示可用的 MCP 工具：

```
╔══════════════════════════════════════════════════════════════╗
║                    服务器已启动                               ║
╠══════════════════════════════════════════════════════════════╣
║  MCP 服务器监听端口：9090
║                                                              ║
║  可用工具：                                                   ║
║    • UserServiceRpc_Login                                   ║
║    • UserServiceRpc_Register                                ║
╚══════════════════════════════════════════════════════════════╝
```

### 通过 MCP 协议发现函数

你的本地大模型可以通过 JSON-RPC 2.0 连接到 MCP 服务器：

**1. 初始化会话**
```bash
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | nc localhost 9090
```

**2. 发现可用的函数（tools/list）**
```bash
echo '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | nc localhost 9090
```

响应示例：
```json
{
  "result": {
    "tools": [
      {
        "name": "UserServiceRpc_Login",
        "description": "[RPC] Kuser.UserServiceRpc.Login — input: LoginRequest, output: LoginResponse",
        "inputSchema": {...}
      },
      {
        "name": "UserServiceRpc_Register", 
        "description": "[RPC] Kuser.UserServiceRpc.Register — input: RegisterRequest, output: RegisterResponse",
        "inputSchema": {...}
      }
    ]
  }
}
```

**3. 调用函数（tools/call）**
```bash
echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"UserServiceRpc_Login","arguments":{"name":"dGVzdA==","pwd":"MTIzNDU2"}}}' | nc localhost 9090
```

### 基本使用（代码示例）

```cpp
#include "McpRpcBridge.h"

int main() {
    // 1. 创建工具注册表
    mcp_rpc::ToolRegistry registry;

    // 2. 注册 RPC 服务（自动注册所有方法）
    const auto* desc = MyRpcService::descriptor();
    registry.RegisterService(desc);

    // 3. 创建 RPC Invoker
    auto invoker = mcp_rpc::MakeKrpcInvoker();

    // 4. 创建 MCP Session
    mcp_rpc::StreamingConfig config;
    config.aggregate_max_bytes = 1 * 1024 * 1024;  // 1MB 聚合限制
    mcp_rpc::MCPSession session(registry, invoker, config);

    // 5. 处理 MCP 请求
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"},
        {"params", {}}
    };

    auto response = session.HandleRequest(request);
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
│  │    StreamingResponseAggregator (流式响应聚合)           │ │
│  └─────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │         ArgumentsDeserializer (JSON → Protobuf)         │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ RpcInvoker 接口
                              │ (serialized bytes in/out)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Transport Layer                         │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐ │
│  │KrpcInvoker  │  │ MockInvoker  │  │ (更多 Invoker...)    │ │
│  │(Krpc 框架)   │  │ (测试用)     │  │                     │ │
│  └─────────────┘  └──────────────┘  └─────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Backend RPC Services                       │
└─────────────────────────────────────────────────────────────┘
```

## 目录结构

```
McpRpcBridge/
├── src/
│   ├── mcp/                        # MCP 适配层
│   │   ├── LazyValue.h             # 线程安全懒加载
│   │   ├── ToolRegistry.h/cc       # 工具注册表
│   │   ├── ProtoSchemaConverter.h/cc  # Protobuf ↔ JSON Schema
│   │   ├── ArgumentsDeserializer.h/cc # JSON → Protobuf
│   │   ├── MCPSession.h/cc         # JSON-RPC 会话管理
│   │   ├── StreamingConfig.h       # 流式 RPC 配置
│   │   └── StreamingResponseAggregator.h/cc  # 流式响应聚合
│   ├── invoker/                    # RPC 调用器层
│   │   ├── RpcInvoker.h            # 抽象接口
│   │   ├── KrpcInvoker.h/cc        # Krpc 实现
│   │   └── MockRpcInvoker.h/cc     # Mock 实现（测试用）
│   └── McpRpcBridge.h              # 总入口头文件
├── example/
│   ├── server/main.cc              # 示例服务器
│   └── client/main.cc              # 示例客户端
├── test/                           # 单元测试
│   ├── test_lazy_value.cc
│   ├── test_tool_registry.cc
│   ├── test_schema_converter.cc
│   ├── test_mcp_session.cc
│   ├── test_rpc_invoker.cc
│   ├── test_krpc_invoker.cc
│   ├── test_arguments_deserializer.cc
│   ├── test_streaming_aggregator.cc
│   ├── test_mcp_session_streaming.cc
│   └── test_integration_e2e.cc     # 端到端集成测试
└── CMakeLists.txt
```

## 类型映射

| Protobuf 类型 | JSON Schema | MCP 工具参数 |
|-------------|-------------|-------------|
| `string` | `{"type": "string"}` | 字符串 |
| `bytes` | `{"type": "string", "format": "base64"}` | Base64 字符串 |
| `int32/64`, `uint32/64` | `{"type": "integer"}` | 数字 |
| `float/double` | `{"type": "number"}` | 数字 |
| `bool` | `{"type": "boolean"}` | 布尔值 |
| `enum` | `{"type": "string", "enum": [...]}` | 枚举字符串 |
| `message` | `{"$ref": "#/$defs/TypeName"}` | 嵌套对象 |
| `repeated T` | `{"type": "array", "items": {...}}` | 数组 |
| `map<K,V>` | `{"type": "object", "additionalProperties": {...}}` | 对象 |
| `oneof` | `{"oneOf": [...]}` | 联合类型 |

## 流式 RPC 处理

MCP-RPC Bridge 支持 server-streaming RPC 的自动聚合：

```cpp
mcp_rpc::StreamingConfig config;
config.aggregate_max_bytes = 10 * 1024 * 1024;  // 10MB 聚合限制

// 聚合模式：
// - JSON_ARRAY: 将多个流式消息聚合成 JSON 数组（默认）
// - CONCAT: 对于 string 类型，连接所有字符串
mcp_rpc::MCPSession session(registry, invoker, config);
```

### 流式响应示例

```json
// 输入：3 个 StreamResponse { data: "chunk_1" }, { data: "chunk_2" }, { data: "chunk_3" }
// 输出:
{
  "result": {
    "content": {
      "type": "array",
      "streaming": true,
      "data": [
        {"data": "chunk_1"},
        {"data": "chunk_2"},
        {"data": "chunk_3"}
      ]
    }
  }
}
```

## API 参考

### MCPSession

```cpp
class MCPSession {
public:
    // 创建会话
    MCPSession(ToolRegistry& registry, 
               std::shared_ptr<RpcInvoker> invoker,
               const StreamingConfig& config = {});

    // 处理 JSON-RPC 请求
    nlohmann::json HandleRequest(const nlohmann::json& request);

    // 获取当前会话状态
    SessionState GetState() const;  // New | Initialized | Closed
};
```

### ToolRegistry

```cpp
class ToolRegistry {
public:
    // 注册服务（自动注册所有方法）
    void RegisterService(const google::protobuf::ServiceDescriptor* service);

    // 查找工具
    const ToolEntry* FindTool(const std::string& name) const;

    // 列出所有工具
    std::vector<const ToolEntry*> ListTools() const;

    size_t Size() const;
};
```

### RpcInvoker

```cpp
// 创建 Invoker
std::shared_ptr<RpcInvoker> MakeKrpcInvoker(bool lazy_connect = true);
std::shared_ptr<RpcInvoker> MakeMockRpcInvoker();  // 测试用

// 调用 RPC
virtual Status Invoke(const std::string& method,
                      const std::vector<uint8_t>& request_bytes,
                      std::vector<uint8_t>* response_bytes) = 0;
```

## 测试

### 运行所有测试

```bash
cd build
ctest --output-on-failure
```

### 测试覆盖

| 测试模块 | 测试数 | 覆盖功能 |
|---------|--------|---------|
| LazyValue | 12 | 懒加载值、线程安全 |
| ToolRegistry | 15 | 服务注册、工具发现 |
| SchemaConverter | 20 | Protobuf ↔ JSON Schema |
| MCPSession | 10 | JSON-RPC 协议 |
| RpcInvoker | 8 | RPC 调用 |
| KrpcInvoker | 10 | Krpc 集成 |
| ArgumentsDeserializer | 12 | JSON → Protobuf |
| StreamingAggregator | 35 | 流式响应聚合 |
| McpSessionStreaming | 15 | 流式 RPC |
| **IntegrationE2E** | **23** | **端到端集成** |

**总计：160+ 个测试用例，100% 通过**

## 错误处理

JSON-RPC 错误码：

| 错误码 | 说明 |
|-------|------|
| -32600 | Invalid Request - 无效的 JSON-RPC 请求 |
| -32601 | Method Not Found - 未知方法/工具 |
| -32602 | Invalid Params - 参数验证失败 |
| -32603 | Internal Error - RPC 调用失败 |

## 自定义 FieldOptions

在 proto 文件中定义自定义选项以增强 Schema 生成：

```protobuf
extend google.protobuf.FieldOptions {
    string description = 50001;  // 字段描述
    string example     = 50002;  // 示例值
    bool   required    = 50003;  // 必填字段
    string format      = 50004;  // JSON Schema format (email, uri, etc.)
}

message CreateUserRequest {
    string email = 1 [(description) = "用户邮箱", (format) = "email", (required) = true];
    string name  = 2 [(description) = "用户昵称", (example) = "Alice"];
}
```

## 性能指标

| 指标 | 值 |
|-----|---|
| 核心库大小 | ~2,400 行 |
| Schema 生成延迟 | <1ms (懒加载缓存) |
| JSON → Protobuf | ~100ns/字段 |
| 内存占用 | <10MB (基础运行时) |

## 配置

创建配置文件 `rpc.conf`：

```conf
rpcserverip=127.0.0.1
rpcserverport=8000
zookeeperip=127.0.0.1
zookeeperport=2181
```

运行服务器时指定配置文件路径：
```bash
./example/server/mcp_example_server -i path/to/rpc.conf
```

## 版本历史

### v1.0.0 (2026-03-31)

- ✅ 完整的 MCP 协议支持
- ✅ Unary 和 Server-Streaming RPC
- ✅ KrpcInvoker 完整实现
- ✅ 端到端集成测试
- ✅ 160+ 测试用例
- ✅ MCP 服务器示例程序（支持 LLM 直接连接）

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！

主要联系人：cannj
