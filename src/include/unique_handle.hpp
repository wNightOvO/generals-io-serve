/*
    智能句柄
    用 RAII 管理 Windows 句柄 防止泄漏
*/

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <utility>

namespace gen {

// Windows 句柄的独占所有权包装 禁止拷贝 允许移动
class UniqueHandle {
private:
    HANDLE handle_ = nullptr; // 保存的句柄

    // 判断句柄是否需要在析构时关闭 进程伪句柄和无效句柄不需要关闭
    static bool shouldClose(HANDLE handle) {
        if(handle == nullptr) return false;
        if(handle == INVALID_HANDLE_VALUE) return false;
        if(handle == GetCurrentProcess()) return false;
        if(handle == GetCurrentThread()) return false;
        return true;
    }
public:
    UniqueHandle() = default;

    // 接管一个句柄
    explicit UniqueHandle(HANDLE handle) noexcept: handle_(handle) {}

    UniqueHandle(const UniqueHandle&) = delete;

    UniqueHandle& operator = (const UniqueHandle&) = delete;

    ~UniqueHandle() {
        reset();
    }

    // 移动构造 转移所有权
    UniqueHandle(UniqueHandle &&other) noexcept: handle_(other.release()) {}

    // 移动赋值 先关闭当前句柄再接管
    UniqueHandle& operator = (UniqueHandle &&other) noexcept {
        if(this != &other) reset(other.release());
        return *this;
    }

    // 放弃所有权并返回裸句柄 不再负责关闭
    HANDLE release() noexcept {
        HANDLE handle = handle_;
        handle_ = nullptr;
        return handle;
    }

    // 关闭旧句柄并接管新句柄
    void reset(HANDLE handle = nullptr) noexcept {
        if(shouldClose(handle_)) CloseHandle(handle_);
        handle_ = handle;
    }

    // 返回裸句柄
    HANDLE get() const noexcept {
        return handle_;
    }

    // 判断句柄是否有效
    explicit operator bool() const noexcept {
        return shouldClose(handle_);
    }

    // 判断句柄是否有效
    bool valid() const noexcept {
        return shouldClose(handle_);
    }
};

} // namespace gen
