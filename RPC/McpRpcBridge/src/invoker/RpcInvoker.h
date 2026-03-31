#ifndef MCP_RPC_BRIDGE_RPC_INVOKER_H
#define MCP_RPC_BRIDGE_RPC_INVOKER_H

#include <string>
#include <vector>
#include <future>
#include <memory>
#include <variant>
#include <stdexcept>
#include <cstdint>

#if __cplusplus >= 202002L
#include <span>
#else
// C++17 兼容：使用 pointer + size 替代 span
namespace mcp_rpc {
template<typename T>
class span {
public:
    span() : data_(nullptr), size_(0) {}
    span(T* data, size_t size) : data_(data), size_(size) {}
    span(const std::vector<T>& vec) : data_(const_cast<T*>(vec.data())), size_(vec.size()) {}

    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    T operator[](size_t i) const { return data_[i]; }

private:
    T* data_;
    size_t size_;
};
} // namespace mcp_rpc
#endif

namespace mcp_rpc {

/**
 * @brief 简单的状态码枚举
 */
enum class StatusCode {
    OK = 0,
    ERROR = 1
};

/**
 * @brief 简化的 StatusOr 实现
 *
 * 用于返回可能失败的操作结果。
 */
template<typename T>
class StatusOr {
public:
    // 从值构造
    StatusOr(const T& value) : data_(value), has_error_(false) {}
    StatusOr(T&& value) : data_(std::move(value)), has_error_(false) {}

    // 从错误构造
    StatusOr(StatusCode code, const std::string& msg)
        : data_(ErrorData{code, msg}), has_error_(true) {}

    // 检查是否有错误
    bool ok() const { return !has_error_; }

    // 获取值（如果有）
    const T& value() const {
        if (has_error_) {
            throw std::runtime_error("StatusOr does not contain a value");
        }
        return std::get<T>(data_);
    }

    // 获取错误消息
    std::string message() const {
        if (!has_error_) {
            return "";
        }
        return std::get<ErrorData>(data_).message;
    }

private:
    struct ErrorData {
        StatusCode code;
        std::string message;
    };

    std::variant<T, ErrorData> data_;
    bool has_error_;
};

/**
 * @brief RPC 调用器接口
 *
 * 这是 MCP 层和 RPC 层之间的唯一交叉点。
 */
class RpcInvoker {
public:
    virtual ~RpcInvoker() = default;

    /**
     * @brief 调用 RPC 方法
     */
#if __cplusplus >= 202002L
    virtual std::future<StatusOr<std::vector<uint8_t>>> Invoke(
        std::string_view method_full_name,
        std::span<const uint8_t> request_bytes) = 0;
#else
    virtual std::future<StatusOr<std::vector<uint8_t>>> Invoke(
        const std::string& method_full_name,
        const std::vector<uint8_t>& request_bytes) = 0;
#endif
};

/**
 * @brief RpcInvoker 工厂接口
 */
using RpcInvokerFactory = std::function<std::shared_ptr<RpcInvoker>()>;

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_RPC_INVOKER_H
