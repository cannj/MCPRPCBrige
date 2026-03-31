/**
 * @file main.cc
 * @brief MCP 客户端示例
 *
 * 演示如何作为 LLM 代理调用 MCP 服务。
 */

#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <sstream>

// 简单的 TCP 客户端用于发送 JSON-RPC 请求
class McpClient {
public:
    explicit McpClient(const std::string& host, uint16_t port)
        : host_(host), port_(port) {}

    /**
     * @brief 发送 JSON-RPC 请求并返回响应
     */
    nlohmann::json sendRequest(const std::string& method,
                                const nlohmann::json& params,
                                int id = 1) {
        nlohmann::json request = {
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params},
            {"id", id}
        };

        std::string request_str = request.dump();
        std::cout << "Sending: " << request_str << std::endl;

        // TODO: 实现 TCP 连接和发送/接收
        // 当前仅返回占位响应

        return {
            {"jsonrpc", "2.0"},
            {"result", {{"status", "placeholder"}}},
            {"id", id}
        };
    }

    /**
     * @brief 初始化会话
     */
    nlohmann::json initialize() {
        return sendRequest("initialize", {});
    }

    /**
     * @brief 列出所有工具
     */
    nlohmann::json listTools() {
        return sendRequest("tools/list", {}, 2);
    }

    /**
     * @brief 调用工具
     */
    nlohmann::json callTool(const std::string& name,
                            const nlohmann::json& arguments,
                            int id = 3) {
        return sendRequest("tools/call", {
            {"name", name},
            {"arguments", arguments}
        }, id);
    }

private:
    std::string host_;
    uint16_t port_;
    // int socket_;  // TODO: 实现 socket 连接
};

int main() {
    std::cout << "MCP Client Example" << std::endl;
    std::cout << "==================" << std::endl;

    // 创建客户端（当前未连接）
    McpClient client("localhost", 8888);

    // 步骤 1: 初始化
    std::cout << "\n[1] Initialize session..." << std::endl;
    auto init_resp = client.initialize();
    std::cout << "Response: " << init_resp.dump(2) << std::endl;

    // 步骤 2: 获取工具列表
    std::cout << "\n[2] List tools..." << std::endl;
    auto tools_resp = client.listTools();
    std::cout << "Response: " << tools_resp.dump(2) << std::endl;

    // 步骤 3: 调用工具（示例）
    std::cout << "\n[3] Call tool..." << std::endl;
    auto call_resp = client.callTool("UserServiceRpc_Login", {
        {"name", "test_user"},
        {"pwd", "secret123"}
    });
    std::cout << "Response: " << call_resp.dump(2) << std::endl;

    std::cout << "\nExample completed." << std::endl;

    return 0;
}
