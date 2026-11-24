/*
    智能句柄
*/

#pragma once
#include <windows.h>

namespace gen { // namespace gen

class UniqueHandle {
private:
    HANDLE handle = nullprt;

    static bool shouldClose(HANDLE handle) {
        if(!handle) return false; // 空句柄
        if(handle == INVALID_HANDLE_VALUE) return false; // 无效句柄
        if(handle == GetCurrentProcess()) return false; // 进程伪句柄
        if(handle == GetCurrentThread()) return false; // 线程伪句柄
        return true;
    }
public:
    UniqueHandle() = default;

    // 显式构造
    explicit UniqueHandle(HANDLE handle) noexcept: handle(handle) {}

    // 禁止拷贝
    UniqueHandle(const HANDLE&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    ~UniqueHandle() {reset();}

    // 允许移动构造
    UniqueHandle(UniqueHandle&& o) noexcept : handle(o.release()) {}

    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            reset(o.release());
        }
        return *this;
    }

    // 放弃所有权，返回裸句柄，自己不关
    HANDLE release() noexcept {
        HANDLE t = handle;
        handle = nullptr;
        return t;
    }

    // 先关旧的，再接管新的
    void reset(HANDLE h = nullptr) noexcept {
        if (shouldClose(handle)) {
            CloseHandle(handle);
        }
        handle = h;
    }

    HANDLE get() const noexcept {return handle;}

    // 转化为 bool 判断句柄是否有效
    explicit operator bool() const noexcept {return shouldClose(handle);}
    bool valid() const noexcept {return shouldClose(handle);}
};

} // namespace gen