#include "game.hpp"

#include <sstream>

namespace voxel {

Game::Game(GameConfig config) : config_(std::move(config)) {}

bool Game::initialize(std::string& error) {
    PlatformConfig platformConfig;
    platformConfig.width = config_.width;
    platformConfig.height = config_.height;
    platformConfig.title = "VoxelCraft";
    if (!platform_.create(platformConfig, error)) {
        return false;
    }

    if (!renderer_.initialize(error)) {
        return false;
    }

    WorldConfig worldConfig;
    worldConfig.worldName = config_.worldName;
    worldConfig.seed = config_.seed;
    worldConfig.saveRoot = config_.saveRoot;
    session_.emplace(worldConfig);
    session_->initialize();
    return true;
}

int Game::run() {
    while (platform_.isOpen()) {
        const PlatformFrame frame = platform_.pollFrame();
        if (frame.input.quitRequested) {
            break;
        }

        const float deltaTime = clampf(static_cast<float>(frame.deltaTime), 0.0f, 0.05f);
        session_->update(deltaTime, frame.input);
        renderer_.render(*session_, frame.width, frame.height);
        platform_.swapBuffers();
        updateTitle(frame.deltaTime);
    }

    if (session_.has_value()) {
        session_->shutdown();
    }
    return 0;
}

void Game::updateTitle(double deltaTime) {
    if (!session_.has_value()) {
        return;
    }

    titleTimer_ += deltaTime;
    ++titleFrames_;
    if (titleTimer_ < 0.5) {
        return;
    }

    const double fps = static_cast<double>(titleFrames_) / titleTimer_;
    std::ostringstream title;
    title.precision(0);
    title << std::fixed << "VoxelCraft - " << fps << " FPS - world: " << session_->world().worldName();
    if (session_->inventoryOpen()) {
        title << " [inv]";
    } else if (session_->player().flying) {
        title << " [fly]";
    }
    platform_.setTitle(title.str());
    titleTimer_ = 0.0;
    titleFrames_ = 0;
}

}  // namespace voxel
