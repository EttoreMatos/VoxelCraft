#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "platform.hpp"
#include "renderer.hpp"
#include "session.hpp"

namespace voxel {

struct GameConfig {
    std::string worldName = "default";
    std::string minecraftSource;
    std::uint32_t seed = 0;
    int width = 960;
    int height = 600;
    std::filesystem::path saveRoot = "saves";
};

class Game {
public:
    explicit Game(GameConfig config);

    bool initialize(std::string& error);
    int run();

private:
    void updateTitle(double deltaTime);

    GameConfig config_;
    Platform platform_;
    Renderer renderer_;
    std::optional<GameSession> session_;
    double titleTimer_ = 0.0;
    int titleFrames_ = 0;
};

}  // namespace voxel
