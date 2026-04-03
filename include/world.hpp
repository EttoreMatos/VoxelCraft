#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>

#include "common.hpp"
#include "persistence.hpp"

namespace voxel {

struct WorldConfig {
    std::string worldName = "default";
    std::uint32_t seed = 0;
    std::filesystem::path saveRoot = "saves";
    int activeRadius = kActiveRadius;
    int renderRadius = kRenderRadius;
};

struct Chunk {
    ChunkCoord coord;
    std::array<BlockId, kChunkVolume> blocks {};
    MeshData mesh;
    bool modified = false;
    bool meshDirty = true;

    static constexpr int index(int localX, int localY, int localZ) {
        return localY * kChunkSizeX * kChunkSizeZ + localZ * kChunkSizeX + localX;
    }

    [[nodiscard]] BlockId get(int localX, int localY, int localZ) const {
        return blocks[static_cast<std::size_t>(index(localX, localY, localZ))];
    }

    void set(int localX, int localY, int localZ, BlockId block) {
        blocks[static_cast<std::size_t>(index(localX, localY, localZ))] = block;
    }
};

class World {
public:
    explicit World(WorldConfig config);

    [[nodiscard]] const WorldConfig& config() const {
        return config_;
    }

    [[nodiscard]] std::uint32_t seed() const {
        return seed_;
    }

    [[nodiscard]] const std::string& worldName() const {
        return persistence_.worldName();
    }

    [[nodiscard]] std::filesystem::path savePath() const {
        return persistence_.savePath();
    }

    [[nodiscard]] std::optional<PlayerState> initialPlayerState() const;
    void storePlayerState(const PlayerState& player);

    void warmStart(const ChunkCoord& center, int radius = 1);
    void updateStreaming(const ChunkCoord& center);
    [[nodiscard]] std::vector<const Chunk*> collectRenderable(const ChunkCoord& center) const;

    [[nodiscard]] BlockId getBlock(int worldX, int worldY, int worldZ) const;
    bool setBlock(int worldX, int worldY, int worldZ, BlockId block);

    [[nodiscard]] int terrainHeight(int worldX, int worldZ) const;
    [[nodiscard]] Vec3 spawnPosition() const;

    [[nodiscard]] bool collidesAabb(const Vec3& min, const Vec3& max) const;
    [[nodiscard]] std::optional<RaycastHit> raycast(const Vec3& origin, const Vec3& direction,
                                                    float maxDistance) const;

    void flushAll();

private:
    void loadOrCreateMeta(std::uint32_t requestedSeed);
    [[nodiscard]] BlockId generatedBlock(int worldX, int worldY, int worldZ) const;
    [[nodiscard]] bool treeAt(int worldX, int worldZ) const;

    Chunk* findChunk(const ChunkCoord& coord);
    [[nodiscard]] const Chunk* findChunk(const ChunkCoord& coord) const;
    Chunk& ensureChunkLoaded(const ChunkCoord& coord);
    void loadChunk(Chunk& chunk);
    void generateChunk(Chunk& chunk) const;
    void rebuildChunkMesh(Chunk& chunk);
    void markChunkAndNeighborsDirty(const ChunkCoord& coord);
    void unloadFarChunks(const ChunkCoord& center);
    [[nodiscard]] bool loadOneMissingChunk(const ChunkCoord& center);
    [[nodiscard]] bool rebuildOneDirtyChunk(const ChunkCoord& center);

    WorldConfig config_;
    WorldPersistence persistence_;
    std::uint32_t seed_ = 0;
    std::optional<PlayerState> savedPlayerState_;
    bool metaDirty_ = false;
    std::map<ChunkCoord, std::unique_ptr<Chunk>> loadedChunks_;
};

}  // namespace voxel
