#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "common.hpp"

namespace voxel {

class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    bool initialize(const std::string& minecraftSource, std::string& warningOrError);
    void shutdown();

    [[nodiscard]] bool available() const {
        return available_;
    }

    [[nodiscard]] unsigned int textureId() const {
        return textureId_;
    }

    [[nodiscard]] UVRect blockFace(BlockId block, int face) const {
        return blockFaceUv(block, face);
    }

    [[nodiscard]] UVRect blockIcon(BlockId block) const {
        return blockIconUv(block);
    }

private:
    bool prepareFromSource(const std::filesystem::path& source, std::string& warningOrError);
    bool loadGeneratedAtlas(std::string& warningOrError);
    bool uploadRgba(const std::vector<std::uint8_t>& pixels, int width, int height, std::string& warningOrError);

    unsigned int textureId_ = 0;
    bool available_ = false;
};

}  // namespace voxel
