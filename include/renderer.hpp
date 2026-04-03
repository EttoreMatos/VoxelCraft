#pragma once

#include <string>

#include "session.hpp"

namespace voxel {

class Renderer {
public:
    bool initialize(std::string& error);
    void render(const GameSession& session, int width, int height) const;
};

}  // namespace voxel
