#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "common.hpp"

namespace voxel {

struct WorldMeta {
    std::uint32_t seed = 0;
    PlayerState player;
    InventoryState inventory;
    bool hasPlayerState = false;
    bool hasInventory = false;
};

class WorldPersistence {
public:
    static constexpr std::uint32_t kMetaVersion = 3;
    static constexpr std::uint32_t kChunkVersion = 2;

    WorldPersistence(std::filesystem::path saveRoot, std::string worldName);

    [[nodiscard]] const std::string& worldName() const {
        return worldName_;
    }

    [[nodiscard]] std::filesystem::path savePath() const {
        return savePath_;
    }

    [[nodiscard]] bool loadMeta(WorldMeta& meta) const;
    bool saveMeta(const WorldMeta& meta) const;

    [[nodiscard]] bool loadChunk(const ChunkCoord& coord, std::array<BlockId, kChunkVolume>& blocks) const;
    bool saveChunk(const ChunkCoord& coord, const std::array<BlockId, kChunkVolume>& blocks) const;

private:
    [[nodiscard]] std::filesystem::path chunkFilePath(const ChunkCoord& coord) const;

    std::string worldName_;
    std::filesystem::path saveRoot_;
    std::filesystem::path savePath_;
    std::filesystem::path chunkPath_;
    std::filesystem::path metaPath_;
};

}  // namespace voxel
