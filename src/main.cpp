#include <windows.h>
#include <iostream>
#include <core/game.hpp>
#include <io/io.hpp>
#include <include/logger.hpp>

namespace gen {
	
}

int main() {
    SetConsoleOutputCP(CP_UTF8); // 设为UTF-8编码
	gen::logger.info("Hello World!");
    return 0;
}
