g++ -std=gnu++20 -static -o serve.exe -Wall -Isrc -Ilib -Llib/easyx/lib src/*.cpp src/core/*.cpp src/io/*.cpp src/ui/*.cpp -leasyxw
pause