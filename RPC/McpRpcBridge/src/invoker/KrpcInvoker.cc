#include "invoker/KrpcInvoker.h"
// TODO: 添加 KrpcChannel 头文件路径
// #include "Krpcchannel.h"
#include <future>
#include <stdexcept>
#include <sstream>

namespace mcp_rpc {

KrpcInvoker::KrpcInvoker() : connect_now_(false) {}

KrpcInvoker::~KrpcInvoker() = default;

std::future<StatusOr<std::vector<uint8_t>>> KrpcInvoker::Invoke(
    const std::string& method_full_name,
    const std::vector<uint8_t>& request_bytes) {

    // 使用 async 包裹同步调用，返回 future
    return std::async(std::launch::async, [this, method_full_name = std::string(method_full_name),
                                            request_data = request_bytes]()
        -> StatusOr<std::vector<uint8_t>>
    {
        try {
            // TODO: 获取或创建 Channel
            // auto* channel = GetOrCreateChannel(method_full_name);
            // if (!channel) {
            //     return StatusOr<std::vector<uint8_t>>(
            //         StatusCode::ERROR,
            //         "Failed to get channel for: " + method_full_name);
            // }

            // TODO: 解析方法名
            // std::string package, service, method;
            // if (!ParseMethodName(method_full_name, package, service, method)) {
            //     return StatusOr<std::vector<uint8_t>>(
            //         StatusCode::ERROR,
            //         "Invalid method name format: " + method_full_name);
            // }

            // TODO: 调用 KrpcChannel 的 CallMethod
            // 当前返回一个占位实现

            std::vector<uint8_t> response;
            return response;

        } catch (const std::exception& e) {
            return StatusOr<std::vector<uint8_t>>(
                StatusCode::ERROR,
                std::string("Exception during invoke: ") + std::string(e.what()));
        }
    });
}

// KrpcChannel* KrpcInvoker::GetOrCreateChannel(const std::string& method_full_name) {
//     auto it = channels_.find(method_full_name);
//     if (it != channels_.end()) {
//         return it->second.get();
//     }
//     auto channel = std::make_unique<KrpcChannel>(connect_now_);
//     auto* raw = channel.get();
//     channels_[method_full_name] = std::move(channel);
//     return raw;
// }

// bool KrpcInvoker::ParseMethodName(const std::string& full_name,
//                                    std::string& package,
//                                    std::string& service,
//                                    std::string& method) {
//     auto slash_pos = full_name.find('/');
//     if (slash_pos == std::string::npos) {
//         return false;
//     }
//     method = full_name.substr(slash_pos + 1);
//     std::string service_part = full_name.substr(0, slash_pos);
//     auto dot_pos = service_part.find('.');
//     if (dot_pos != std::string::npos) {
//         package = service_part.substr(0, dot_pos);
//         service = service_part.substr(dot_pos + 1);
//     } else {
//         package = "";
//         service = service_part;
//     }
//     return true;
// }

std::shared_ptr<RpcInvoker> MakeKrpcInvoker(bool connect_now) {
    auto invoker = std::make_shared<KrpcInvoker>();
    invoker->SetConnectNow(connect_now);
    return invoker;
}

} // namespace mcp_rpc
