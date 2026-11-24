# generals-io-serve

这是一个模拟 generals.io 的非官方服务器。

本项目为非官方第三方工具，与 generals.io 官方无关。

## 编译环境

MinGW GCC 16.2.0。

C++20。

编译选项： `g++ -std=gnu++20 -static -o serve.exe -Wall -Isrc -Ilib -Llib/easyx/lib src/*.cpp src/core/*.cpp src/io/*.cpp src/ui/*.cpp -leasyx -lgdi32 -lole32 -lwinmm`。

## 生成文件
生成在本目录下的 serve.exe 中。