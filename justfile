build:
    cmake -G "Ninja Multi-Config" -S . -B build/
    ninja -C build/
    cp ./build/compile_commands.json .

run: build
    ./build/Debug/learn-vk
