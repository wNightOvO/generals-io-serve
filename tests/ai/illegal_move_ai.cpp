/*
    测试用非法移动 AI
    初始化正常 每回合收到 GO 后输出语法合法但棋盘上非法的动作
    动作 (0,0) 到 (0,2) 曼哈顿距离为 2 不满足相邻要求
    用于验证游戏程序能识别非法移动并计违规 最终判负
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
            writeLine("M 0 0 0 2");
        }
        else if(type == MSG_GAME_OVER) {
            return 0;
        }
    }
    return 0;
}
