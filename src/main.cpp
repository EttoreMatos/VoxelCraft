#include "game.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cout << "Uso: ./build/voxelcraft [--world nome] [--seed inteiro] [--width px] [--height px]\n";
}

}  // namespace

int main(int argc, char** argv) {
    voxel::GameConfig config;
    config.seed = static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xffffffffULL);

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto requireValue = [&](const std::string& flag) -> const char* {
            if (index + 1 >= argc) {
                std::cerr << "Faltou valor para " << flag << ".\n";
                printUsage();
                std::exit(1);
            }
            return argv[++index];
        };

        if (arg == "--world") {
            config.worldName = requireValue(arg);
        } else if (arg == "--seed") {
            config.seed = static_cast<std::uint32_t>(std::stoul(requireValue(arg)));
        } else if (arg == "--width") {
            config.width = std::max(320, std::stoi(requireValue(arg)));
        } else if (arg == "--height") {
            config.height = std::max(240, std::stoi(requireValue(arg)));
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Argumento desconhecido: " << arg << '\n';
            printUsage();
            return 1;
        }
    }

    voxel::Game game(config);
    std::string error;
    if (!game.initialize(error)) {
        std::cerr << error << '\n';
        return 1;
    }

    return game.run();
}
