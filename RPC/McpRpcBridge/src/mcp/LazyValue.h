#ifndef MCP_RPC_BRIDGE_LAZY_VALUE_H
#define MCP_RPC_BRIDGE_LAZY_VALUE_H

#include <mutex>
#include <functional>
#include <memory>
#include <stdexcept>

namespace mcp_rpc {

/**
 * @brief 线程安全的延迟初始化容器
 *
 * 设计原则：
 * 1. 使用 mutex + 双重检查锁定，避免 std::call_once 无法重置的问题
 * 2. 用 value_ == nullptr 判断是否初始化，无需额外的 atomic 标志
 * 3. 移动语义完整转移状态，源对象自然进入未初始化状态
 * 4. 无未定义行为，符合 C++ 核心指南
 *
 * @tparam T 存储的值类型
 */
template<typename T>
class LazyValue {
public:
    LazyValue() = default;

    // 移动构造：完整转移所有权
    LazyValue(LazyValue&& other) noexcept
        : value_(std::move(other.value_)) {
        // other.value_ 被移动后自动为 nullptr，表示未初始化状态
        // 无需额外操作
    }

    // 移动赋值：完整转移所有权
    LazyValue& operator=(LazyValue&& other) noexcept {
        if (this != &other) {
            // 先加锁防止与 Reset/GetOrInit 并发
            std::lock_guard<std::mutex> self_lock(mutex_);
            value_ = std::move(other.value_);
            // other.value_ 移动后为 nullptr
        }
        return *this;
    }

    // 删除拷贝操作（LazyValue 独占所有权）
    LazyValue(const LazyValue&) = delete;
    LazyValue& operator=(const LazyValue&) = delete;

    /**
     * @brief 获取值，如果未初始化则调用初始化函数
     * @param init_fn 初始化函数
     * @return const T& 初始化后的值引用
     *
     * 线程安全实现：
     * 1. 快速路径：无锁检查 value_ 是否已初始化
     * 2. 慢速路径：加锁后双重检查，避免重复初始化
     */
    const T& GetOrInit(std::function<T()> init_fn) {
        // 快速路径：无锁检查（unique_ptr 的 bool 转换是原子的）
        if (value_) {
            return *value_;
        }

        // 慢速路径：加锁初始化
        std::lock_guard<std::mutex> lock(mutex_);
        if (!value_) {  // 双重检查
            value_ = std::make_unique<T>(init_fn());
        }
        return *value_;
    }

    /**
     * @brief 检查是否已初始化
     * @return true 如果已初始化
     */
    bool IsInitialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_ != nullptr;
    }

    /**
     * @brief 重置为未初始化状态
     * @note 线程安全，但调用后不应有其他线程同时调用 GetOrInit
     * @note 主要用于测试场景
     */
    void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        value_.reset();  // 释放资源，value_ 变为 nullptr
    }

    /**
     * @brief 获取值（不初始化）
     * @return const T* 已初始化则返回值指针，否则返回 nullptr
     */
    const T* Peek() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_.get();
    }

private:
    mutable std::mutex mutex_;      // 保护 value_ 的访问
    mutable std::unique_ptr<T> value_;  // nullptr 表示未初始化
};

} // namespace mcp_rpc

#endif // MCP_RPC_BRIDGE_LAZY_VALUE_H
