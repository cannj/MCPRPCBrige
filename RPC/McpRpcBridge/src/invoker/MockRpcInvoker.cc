#include "invoker/MockRpcInvoker.h"
#include <thread>
#include <chrono>

namespace mcp_rpc {

void MockRpcInvoker::SetResponse(const std::string& method_full_name,
                                  const std::vector<uint8_t>& response_bytes) {
    ResponseConfig config;
    config.is_error = false;
    config.response_data = response_bytes;
    method_responses_[method_full_name] = std::move(config);
}

void MockRpcInvoker::SetErrorResponse(const std::string& method_full_name,
                                       StatusCode error_code,
                                       const std::string& error_message) {
    ResponseConfig config;
    config.is_error = true;
    config.error_code = error_code;
    config.error_message = error_message;
    method_responses_[method_full_name] = std::move(config);
}

void MockRpcInvoker::SetDefaultResponse(const std::vector<uint8_t>& response_bytes) {
    default_response_.is_error = false;
    default_response_.response_data = response_bytes;
}

void MockRpcInvoker::SetDefaultErrorResponse(StatusCode error_code,
                                              const std::string& error_message) {
    default_response_.is_error = true;
    default_response_.error_code = error_code;
    default_response_.error_message = error_message;
}

std::future<StatusOr<std::vector<uint8_t>>> MockRpcInvoker::Invoke(
#if __cplusplus >= 202002L
    std::string_view method_full_name_sv,
    std::span<const uint8_t> request_bytes
#else
    const std::string& method_full_name,
    const std::vector<uint8_t>& request_bytes
#endif
) {
    // C++17 兼容：将 string_view 转换为 string
#if __cplusplus >= 202002L
    std::string method_full_name(method_full_name_sv);
#endif

    // 记录调用历史（线程安全）
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        call_history_.emplace_back(method_full_name,
                                    std::vector<uint8_t>(request_bytes.begin(), request_bytes.end()));
    }

    // 增加调用计数（线程安全）
    {
        std::lock_guard<std::mutex> lock(count_mutex_);
        call_counts_[method_full_name]++;
        total_calls_++;
    }

    // 模拟延迟
    if (delay_ms_ > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
    }

    // 创建 promise/future
    std::promise<StatusOr<std::vector<uint8_t>>> promise;

    // 查找方法级别的配置
    auto it = method_responses_.find(method_full_name);
    const ResponseConfig* config = nullptr;
    if (it != method_responses_.end()) {
        config = &it->second;
    } else if (default_response_.is_error || !default_response_.response_data.empty()) {
        config = &default_response_;
    }

    // 生成响应
    if (config == nullptr) {
        // 未配置任何响应，返回错误
        promise.set_value(StatusOr<std::vector<uint8_t>>(
            StatusCode::ERROR,
            "MockRpcInvoker: No response configured for method: " + method_full_name));
    } else if (config->is_error) {
        // 返回错误响应
        promise.set_value(StatusOr<std::vector<uint8_t>>(
            config->error_code,
            config->error_message));
    } else {
        // 返回成功响应
        promise.set_value(StatusOr<std::vector<uint8_t>>(config->response_data));
    }

    return promise.get_future();
}

std::vector<std::pair<std::string, std::vector<uint8_t>>> MockRpcInvoker::GetCallHistory() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return call_history_;
}

int MockRpcInvoker::GetCallCount(const std::string& method_full_name) const {
    std::lock_guard<std::mutex> lock(count_mutex_);
    auto it = call_counts_.find(method_full_name);
    return (it != call_counts_.end()) ? it->second : 0;
}

int MockRpcInvoker::GetTotalCallCount() const {
    std::lock_guard<std::mutex> lock(count_mutex_);
    return total_calls_;
}

void MockRpcInvoker::ClearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    call_history_.clear();

    std::lock_guard<std::mutex> count_lock(count_mutex_);
    call_counts_.clear();
    total_calls_ = 0;
}

void MockRpcInvoker::ClearConfig() {
    std::lock_guard<std::mutex> lock(history_mutex_);  // 也需要锁，防止与 Invoke 并发
    method_responses_.clear();
    default_response_ = ResponseConfig{};
}

std::shared_ptr<MockRpcInvoker> MakeMockRpcInvoker() {
    return std::make_shared<MockRpcInvoker>();
}

} // namespace mcp_rpc
