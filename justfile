[working-directory('assets')]
shaders:
    glslang -g --target-env "vulkan1.3" -V shader.vert -o shader.vert.spv
    glslang -g --target-env "vulkan1.3" -V shader2.vert -o shader2.vert.spv
    glslang -g --target-env "vulkan1.3" -V shader.frag -o shader.frag.spv
    glslang -g --target-env "vulkan1.3" -V shader2.frag -o shader2.frag.spv

build: shaders
    cmake -G "Ninja Multi-Config" -S . -B build/
    ninja -C build; cp ./build/compile_commands.json .

run TARGET: build
    ./bin/{{ TARGET }}
