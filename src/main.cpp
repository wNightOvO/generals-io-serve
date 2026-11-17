#include <windows.h>
#include <iostream>

int main() {
    SetConsoleOutputCP(CP_UTF8); // 设为UTF-8编码
	std::cout << "Hello World" << std::endl;
    return 0;
}
