/*
    测试用崩溃 AI
    初始化正常 收到第一次 GO 后立刻以非零退出码退出
    用于验证游戏程序能识别崩溃并写入日志
*/

#include "protocol_util.hpp"

int main() {
    std::vector<unsigned char> payload;
    while(readFrame(payload)) {
        unsigned char type = payload[0];
        if(type == MSG_INIT) {
            writeLine("READY");
        }
        else if(type == MSG_GO) {
            return 3; // 非零退出码表示异常退出
        }
        else if(type == MSG_GAME_OVER) {
            return 0;
        }
    }
    return 0;
}
