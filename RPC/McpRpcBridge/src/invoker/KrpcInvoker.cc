#include "invoker/KrpcInvoker.h"
#include "Krpcchannel.h"
#include "Krpccontroller.h"
#include <future>
#include <stdexcept>
#include <sstream>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/generated_message_util.h>

namespace mcp_rpc {

KrpcInvoker::KrpcInvoker() : connect_now_(false) {}

KrpcInvoker::~KrpcInvoker() = default;

bool KrpcInvoker::ParseMethodName(const std::string& full_name,
                                   std::string& package,
                                   std::string& service,
                                   std::string& method) {
    // 支持的格式：
    // 1. "/package.Service/Method"
    // 2. "/Service/Method"
    // 3. "package.Service/Method"
    // 4. "Service/Method"

    std::string name = full_name;

    // 移除前导斜杠
    if (!name.empty() && name[0] == '/') {
        name = name.substr(1);
    }

    // 查找最后一个斜杠，分隔服务和方法
    auto slash_pos = name.rfind('/');
    if (slash_pos == std::string::npos) {
        return false;
    }

    method = name.substr(slash_pos + 1);
    std::string service_part = name.substr(0, slash_pos);

    // 查找最后一个点号，分隔包和服务
    auto dot_pos = service_part.rfind('.');
    if (dot_pos != std::string::npos) {
        package = service_part.substr(0, dot_pos);
        service = service_part.substr(dot_pos + 1);
    } else {
        package = "";
        service = service_part;
    }

    // 验证解析结果
    if (service.empty() || method.empty()) {
        return false;
    }

    return true;
}

KrpcChannel* KrpcInvoker::GetOrCreateChannel(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = channels_.find(service_name);
    if (it != channels_.end()) {
        return it->second.get();
    }

    // 创建新的 Channel
    auto channel = std::make_unique<KrpcChannel>(connect_now_);
    auto* raw = channel.get();
    channels_[service_name] = std::move(channel);
    return raw;
}

std::future<StatusOr<std::vector<uint8_t>>> KrpcInvoker::Invoke(
    const std::string& method_full_name,
    const std::vector<uint8_t>& request_bytes) {

    // 使用 async 包裹同步调用，返回 future
    return std::async(std::launch::async, [this, method_full_name = std::string(method_full_name),
                                            request_data = request_bytes]()
        -> StatusOr<std::vector<uint8_t>>
    {
        try {
            // 1. 解析方法名
            std::string package, service, method;
            if (!ParseMethodName(method_full_name, package, service, method)) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Invalid method name format: " + method_full_name);
            }

            // 2. 获取或创建 Channel（按服务名缓存）
            auto* channel = GetOrCreateChannel(service);
            if (!channel) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Failed to get channel for service: " + service);
            }

            // 3. 获取方法描述符
            const google::protobuf::MethodDescriptor* method_desc = nullptr;

            // 通过服务名和方法名查找 MethodDescriptor
            const google::protobuf::ServiceDescriptor* service_desc =
                google::protobuf::DescriptorPool::generated_pool()->FindServiceByName(
                    package.empty() ? service : (package + "." + service));

            if (!service_desc) {
                // 如果不在生成的池中，尝试直接通过全名查找
                service_desc = google::protobuf::DescriptorPool::generated_pool()->FindServiceByName(service);
            }

            if (service_desc) {
                method_desc = service_desc->FindMethodByName(method);
            }

            // 如果仍然找不到，使用通用方式：从 request 类型反推
            // 这是为了支持动态加载的 proto
            if (!method_desc) {
                // 创建一个占位的 method 描述
                // 在实际使用中，KrpcChannel 的 CallMethod 主要依赖序列化的数据
                // MethodDescriptor 主要用于获取元数据
                // 这里我们创建一个虚拟的 descriptor
                // 注意：KrpcChannel 的 CallMethod 实际上只使用了 method 的 name() 和 service()->name()
                // 所以我们需要一个有效的方法描述符
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Cannot find method descriptor for: " + service + "/" + method +
                    ". Make sure the proto file is compiled and linked.");
            }

            // 4. 反序列化请求消息
            // 需要从方法描述符获取请求类型
            const google::protobuf::Descriptor* request_type = method_desc->input_type();
            const google::protobuf::Message* request_prototype =
                google::protobuf::MessageFactory::generated_factory()->GetPrototype(request_type);

            if (!request_prototype) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Cannot get request message prototype for: " + method_full_name);
            }

            std::unique_ptr<google::protobuf::Message> request(request_prototype->New());
            if (!request->ParseFromArray(request_data.data(), request_data.size())) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Failed to parse request message");
            }

            // 5. 创建响应消息原型
            const google::protobuf::Descriptor* response_type = method_desc->output_type();
            const google::protobuf::Message* response_prototype =
                google::protobuf::MessageFactory::generated_factory()->GetPrototype(response_type);

            if (!response_prototype) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Cannot get response message prototype for: " + method_full_name);
            }

            std::unique_ptr<google::protobuf::Message> response(response_prototype->New());

            // 6. 创建 Controller
            Krpccontroller controller;

            // 7. 调用 Channel 的 CallMethod
            channel->CallMethod(method_desc, &controller, request.get(), response.get(), nullptr);

            // 8. 检查是否失败
            if (controller.Failed()) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "RPC call failed: " + controller.ErrorText());
            }

            // 9. 序列化响应
            std::vector<uint8_t> response_data(response->ByteSizeLong());
            if (!response->SerializeToArray(response_data.data(), response_data.size())) {
                return StatusOr<std::vector<uint8_t>>(
                    StatusCode::ERROR,
                    "Failed to serialize response message");
            }

            return response_data;

        } catch (const std::exception& e) {
            return StatusOr<std::vector<uint8_t>>(
                StatusCode::ERROR,
                std::string("Exception during invoke: ") + std::string(e.what()));
        }
    });
}

std::shared_ptr<RpcInvoker> MakeKrpcInvoker(bool connect_now) {
    auto invoker = std::make_shared<KrpcInvoker>();
    invoker->SetConnectNow(connect_now);
    return invoker;
}

} // namespace mcp_rpc
