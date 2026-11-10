#include "Engine.h"

#include <iostream>

int main() {
    voxel::Engine engine;

    if (!engine.init(1280, 720, "C+craft - Voxel Engine Foundation (C++20)")) {
        std::cerr << "Engine initialization failed.\n";
        return 1;
    }

    return engine.run();
}
