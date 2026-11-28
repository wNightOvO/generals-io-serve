/*
    测试用超时 AI
    初始化正常 每回合收到 GO 后故意等待超过回合时限再输出 PASS
    用于验证游戏程序能识别超时并写入日志
*/

#include "protocol_util.hpp"

#include <chrono>
#include <thread>

int main() {
    std::vector<unsigned char> payload;
    while(readFrame(payload)) {
        unsigned char type = payload[0];
        if(type == MSG_INIT) {
            writeLine("READY");
        }
        else if(type == MSG_GO) {
            // 故意睡 500 毫秒 远超过回合时限
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            writeLine("PASS");
        }
        else if(type == MSG_GAME_OVER) {
            return 0;
        }
    }
    return 0;
}
