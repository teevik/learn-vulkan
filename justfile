[working-directory('assets')]
shaders:
    glslang -g --target-env "vulkan1.3" -V shader.vert
    glslang -g --target-env "vulkan1.3" -V shader.frag

build: shaders
    cmake -G "Ninja Multi-Config" -S . -B build/
    ninja -C build/
    cp ./build/compile_commands.json .

run TARGET: build
    ./bin/{{ TARGET }}
