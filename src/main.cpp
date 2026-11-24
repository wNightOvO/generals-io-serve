#include <windows.h>
#include <iostream>
#include <core/game.hpp>
#include <io/io.hpp>
#include <ui/ui.hpp>
#include <include/logger.hpp>

namespace { // namespace

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8); // 设为UTF-8编码
	gen::logger.info("Hello World!");
    system("pause");
    return 0;
}