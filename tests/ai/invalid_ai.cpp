/*
    测试用非法输出 AI
    初始化正常 每回合收到 GO 后输出不符合协议的一行文本
    用于验证游戏程序能识别非法输出并写入日志
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
            writeLine("THIS IS NOT A VALID ACTION");
        }
        else if(type == MSG_GAME_OVER) {
            return 0;
        }
    }
    return 0;
}
