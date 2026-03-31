/**
 * @file main.cc
 * @brief MCP-RPC Bridge 服务器 - 支持 MCP 客户端通过 TCP 连接
 *
 * 启动后，MCP 客户端可以通过 TCP 连接到指定端口，发送 JSON-RPC 2.0 请求。
 *
 * MCP 客户端可以发现并调用以下函数：
 * - UserServiceRpc_Login
 * - UserServiceRpc_Register
 */

#include "McpRpcBridge.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <fstream>

// 包含 Krpc 示例的 proto 文件和框架
#include "user.pb.h"
#include "Krpcapplication.h"
#include "Krpcprovider.h"
#include "Krpcchannel.h"
#include "Krpccontroller.h"
#include "invoker/MockRpcInvoker.h"

// 简单的 TCP 服务器头文件（内联实现）
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>

using namespace mcp_rpc;

// 全局标志用于优雅退出
std::atomic<bool> g_running(true);

void signal_handler(int signum) {
    std::cout << "\n收到退出信号，正在关闭服务器..." << std::endl;
    g_running = false;
}

// 全局 session 和 registry（用于 MCP 请求处理）
static ToolRegistry* g_registry = nullptr;
static MCPSession* g_session = nullptr;

/**
 * @brief 处理单个 MCP 客户端连接
 */
void handle_client(int client_fd) {
    char buffer[65536];  // 64KB 缓冲区
    std::string pending_data;  // 用于处理粘包

    std::cout << "  [MCP] 客户端连接建立 (fd=" << client_fd << ")" << std::endl;

    while (g_running) {
        memset(buffer, 0, sizeof(buffer));

        // 读取一行数据（以\n 结尾）
        int total_bytes = 0;
        bool found_newline = false;

        while (g_running && !found_newline) {
            int bytes_read = read(client_fd, buffer + total_bytes, 1);
            if (bytes_read <= 0) {
                // 客户端断开或错误
                if (bytes_read == 0) {
                    std::cout << "  [MCP] 客户端断开连接 (fd=" << client_fd << ")" << std::endl;
                } else {
                    std::cerr << "  [MCP] 读取错误 (fd=" << client_fd << ")" << std::endl;
                }
                close(client_fd);
                return;
            }

            total_bytes += bytes_read;

            // 检查是否收到换行符
            if (buffer[total_bytes - 1] == '\n') {
                found_newline = true;
            }

            // 防止缓冲区溢出
            if (total_bytes >= sizeof(buffer) - 2) {
                std::cerr << "  [MCP] 请求过大 (fd=" << client_fd << ")" << std::endl;
                close(client_fd);
                return;
            }
        }

        // 去掉末尾的换行符
        if (total_bytes > 0 && buffer[total_bytes - 1] == '\n') {
            buffer[total_bytes - 1] = '\0';
            total_bytes--;
        }

        std::string request_str(buffer, total_bytes);
        std::cout << "  [MCP] 收到请求：" << request_str << std::endl;

        try {
            // 解析 JSON 请求
            nlohmann::json request = nlohmann::json::parse(request_str);

            // 处理 MCP 请求
            nlohmann::json response = g_session->HandleRequest(request);

            // 发送响应（如果是 notification 则不发送）
            if (!response.is_null() && request.contains("id")) {
                std::string response_str = response.dump() + "\n";
                write(client_fd, response_str.c_str(), response_str.length());
                std::cout << "  [MCP] 发送响应：" << response.dump(2) << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "  [MCP] 处理请求异常：" << e.what() << std::endl;

            // 发送错误响应
            nlohmann::json error_response = {
                {"jsonrpc", "2.0"},
                {"error", {
                    {"code", -32700},
                    {"message", std::string("Parse error: ") + e.what()}
                }},
                {"id", nullptr}
            };

            std::string response_str = error_response.dump() + "\n";
            write(client_fd, response_str.c_str(), response_str.length());
        }
    }

    close(client_fd);
}

/**
 * @brief MCP 服务器主循环
 */
void run_mcp_server(uint16_t port, ToolRegistry& registry, MCPSession& session) {
    g_registry = &registry;
    g_session = &session;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "无法创建套接字" << std::endl;
        return;
    }

    // 设置 SO_REUSEADDR 选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "无法设置套接字选项" << std::endl;
        close(server_fd);
        return;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "绑定端口失败：" << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "监听失败" << std::endl;
        close(server_fd);
        return;
    }

    std::cout << "[MCP] MCP 服务器已启动，监听端口：" << port << std::endl;
    std::cout << "[MCP] 等待客户端连接..." << std::endl;

    socklen_t addrlen = sizeof(address);

    while (g_running) {
        // 使用 select 实现超时，以便可以响应退出信号
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;  // 1 秒超时
        timeout.tv_usec = 0;

        int activity = select(server_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            if (errno == EINTR) {
                // 被信号中断，继续循环
                continue;
            }
            std::cerr << "select 错误" << std::endl;
            break;
        }

        if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                // 在新线程中处理客户端
                std::thread(handle_client, client_fd).detach();
            }
        }
    }

    close(server_fd);
}

int main(int argc, char* argv[]) {
    // 设置信号处理器
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║              MCP-RPC Bridge 服务器                            ║
║              Version: 1.0.0                                  ║
║                                                              ║
║  支持的 MCP 方法：                                            ║
║    - initialize                                             ║
║    - tools/list                                             ║
║    - tools/call                                             ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
    )" << std::endl;

    // ========================================================================
    // 步骤 1: 初始化 Krpc 框架
    // ========================================================================
    std::cout << "[1] 初始化 Krpc 框架..." << std::endl;
    KrpcApplication::Init(argc, argv);

    // ========================================================================
    // 步骤 2: 创建并注册 RPC 服务到 MCP
    // ========================================================================
    std::cout << "[2] 注册 RPC 服务到 MCP..." << std::endl;

    ToolRegistry registry;

    // 注册 UserServiceRpc 服务（来自 user.proto）
    const auto* desc = Kuser::UserServiceRpc::descriptor();
    registry.RegisterService(desc);

    std::cout << "    已注册 " << registry.Size() << " 个 MCP 工具：" << std::endl;
    for (const auto* tool : registry.ListTools()) {
        std::cout << "      - " << tool->name << std::endl;
    }

    // ========================================================================
    // 步骤 3: 创建 RPC Invoker
    // ========================================================================
    std::cout << "[3] 创建 RPC Invoker..." << std::endl;

    // 注意：这里我们使用 KrpcInvoker，它会通过 KrpcChannel 调用实际的 RPC 服务
    // 如果你有一个实际运行的 Krpc 服务端，Invoker 会连接到它
    // 在演示模式下，我们可以直接调用本地的 UserService 对象

    // 创建本地 UserService 对象用于演示
    class LocalUserService : public Kuser::UserServiceRpc {
    public:
        void Login(::google::protobuf::RpcController* controller,
                  const ::Kuser::LoginRequest* request,
                  ::Kuser::LoginResponse* response,
                  ::google::protobuf::Closure* done) override {
            std::cout << "        [UserService] Login: name=" << request->name()
                      << ", pwd=" << request->pwd() << std::endl;

            Kuser::ResultCode* code = response->mutable_result();
            code->set_errcode(0);
            code->set_errmsg("");
            response->set_success(true);

            done->Run();
        }

        void Register(::google::protobuf::RpcController* controller,
                     const ::Kuser::RegisterRequest* request,
                     ::Kuser::RegisterResponse* response,
                     ::google::protobuf::Closure* done) override {
            std::cout << "        [UserService] Register: id=" << request->id()
                      << ", name=" << request->name() << std::endl;

            Kuser::ResultCode* code = response->mutable_result();
            code->set_errcode(0);
            code->set_errmsg("");
            response->set_success(true);

            done->Run();
        }
    };

    // 对于演示，我们创建一个简单的 MockInvoker 来直接调用本地服务
    auto invoker = std::make_shared<MockRpcInvoker>();

    // 配置 MockInvoker 来模拟 Login 和 Register 的响应
    // 在实际部署中，你会使用 MakeKrpcInvoker() 连接到真实的 Krpc 服务

    // 创建 Login 响应
    Kuser::LoginResponse login_resp;
    login_resp.mutable_result()->set_errcode(0);
    login_resp.mutable_result()->set_errmsg("success");
    login_resp.set_success(true);
    std::vector<uint8_t> login_bytes(login_resp.ByteSizeLong());
    login_resp.SerializeToArray(login_bytes.data(), login_bytes.size());
    invoker->SetResponse("Kuser.UserServiceRpc.Login", login_bytes);

    // 创建 Register 响应
    Kuser::RegisterResponse reg_resp;
    reg_resp.mutable_result()->set_errcode(0);
    reg_resp.mutable_result()->set_errmsg("success");
    reg_resp.set_success(true);
    std::vector<uint8_t> reg_bytes(reg_resp.ByteSizeLong());
    reg_resp.SerializeToArray(reg_bytes.data(), reg_bytes.size());
    invoker->SetResponse("Kuser.UserServiceRpc.Register", reg_bytes);

    std::cout << "    Invoker 创建完成（演示模式：MockInvoker）" << std::endl;
    std::cout << "    已配置 Login 和 Register 的模拟响应" << std::endl;

    // ========================================================================
    // 步骤 4: 创建 MCP Session
    // ========================================================================
    std::cout << "[4] 创建 MCP Session..." << std::endl;

    StreamingConfig config;
    config.aggregate_max_bytes = 1 * 1024 * 1024;  // 1MB
    MCPSession session(registry, invoker, config);

    // ========================================================================
    // 步骤 5: 发布 Krpc 服务（可选，用于真实的远程 RPC 调用）
    // ========================================================================
    std::cout << "[5] 关于 Krpc 服务..." << std::endl;
    std::cout << "    注意：当前使用 MockInvoker 模式，不需要启动 Krpc 服务器" << std::endl;
    std::cout << "    如需使用真实 RPC 调用，请修改代码使用 MakeKrpcInvoker()" << std::endl;

    // 如果要启动 Krpc 服务器，需要在主线程运行，因为 muduo EventLoop 有线程限制
    // 当前演示模式不启动 Krpc 服务器，仅使用 MCP 服务

    // ========================================================================
    // 步骤 6: 启动 MCP 服务器
    // ========================================================================
    std::cout << "[6] 启动 MCP 服务器..." << std::endl;

    uint16_t mcp_port = 9090;  // MCP 服务器监听端口

    std::cout << std::endl;
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    服务器已启动                               ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  MCP 服务器监听端口：" << mcp_port << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║  可用工具：                                                   ║" << std::endl;
    for (const auto* tool : registry.ListTools()) {
        std::cout << "║    • " << tool->name << std::endl;
    }
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║  按 Ctrl+C 退出                                               ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    // 运行 MCP 服务器（阻塞）
    run_mcp_server(mcp_port, registry, session);

    std::cout << "服务器已关闭。" << std::endl;

    return 0;
}
