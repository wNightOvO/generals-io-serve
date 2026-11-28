/*
    测试用最小协议读取工具
    供测试用 AI 读取游戏程序发来的长度前缀帧
*/

#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

// 循环读取直到读满指定字节 成功返回 true
inline bool readExact(void *buffer, std::size_t bytes) {
    unsigned char *cursor = static_cast<unsigned char*>(buffer);
    std::size_t remaining = bytes;
    while(remaining > 0) {
        std::size_t got = std::fread(cursor, 1, remaining, stdin);
        if(got == 0) return false;
        cursor += got;
        remaining -= got;
    }
    return true;
}

// 读取一帧负载 负载第一个字节为消息类型 失败返回 false
inline bool readFrame(std::vector<unsigned char> &payload) {
    unsigned char header[4] = {0, 0, 0, 0};
    if(readExact(header, 4) == false) return false;
    std::uint32_t length = static_cast<std::uint32_t>(header[0])
        | (static_cast<std::uint32_t>(header[1]) << 8)
        | (static_cast<std::uint32_t>(header[2]) << 16)
        | (static_cast<std::uint32_t>(header[3]) << 24);
    if(length == 0 || length > 64u * 1024u * 1024u) return false;
    payload.resize(length);
    return readExact(payload.data(), length);
}

// 向标准输出写一行并立即刷新
inline void writeLine(const char *text) {
    std::fputs(text, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// 消息类型编号
inline constexpr unsigned char MSG_INIT = 5;
inline constexpr unsigned char MSG_GO = 7;
inline constexpr unsigned char MSG_GAME_OVER = 8;
