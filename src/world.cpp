#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace voxel {
namespace {

std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

std::uint32_t hash2D(int x, int z, std::uint32_t seed) {
    auto value = static_cast<std::uint32_t>(x) * 0x1f123bb5U;
    value ^= static_cast<std::uint32_t>(z) * 0x5f356495U;
    value ^= seed * 0x9e3779b9U;
    return mix(value);
}

float hash01(int x, int z, std::uint32_t seed) {
    constexpr float scale = 1.0f / static_cast<float>(std::numeric_limits<std::uint32_t>::max());
    return static_cast<float>(hash2D(x, z, seed)) * scale;
}

float smoothStep(float value) {
    return value * value * (3.0f - 2.0f * value);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float valueNoise(float x, float z, std::uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float tx = smoothStep(x - static_cast<float>(x0));
    const float tz = smoothStep(z - static_cast<float>(z0));

    const float v00 = hash01(x0, z0, seed);
    const float v10 = hash01(x1, z0, seed);
    const float v01 = hash01(x0, z1, seed);
    const float v11 = hash01(x1, z1, seed);

    const float a = lerp(v00, v10, tx);
    const float b = lerp(v01, v11, tx);
    return lerp(a, b, tz);
}

float fractalNoise(float x, float z, std::uint32_t seed) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float amplitudeSum = 0.0f;

    for (int octave = 0; octave < 4; ++octave) {
        total += valueNoise(x * frequency, z * frequency, seed + static_cast<std::uint32_t>(octave * 101)) *
                 amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return amplitudeSum > 0.0f ? total / amplitudeSum : 0.0f;
}

int chunkDistanceSquared(const ChunkCoord& a, const ChunkCoord& b) {
    const int dx = a.x - b.x;
    const int dz = a.z - b.z;
    return dx * dx + dz * dz;
}

bool chunkInRadius(const ChunkCoord& center, const ChunkCoord& coord, int radius) {
    return std::abs(coord.x - center.x) <= radius && std::abs(coord.z - center.z) <= radius;
}

void pushVertex(MeshData& mesh, float x, float y, float z, const Color3& color, float shade) {
    mesh.vertices.push_back(x);
    mesh.vertices.push_back(y);
    mesh.vertices.push_back(z);
    mesh.colors.push_back(color.r * shade);
    mesh.colors.push_back(color.g * shade);
    mesh.colors.push_back(color.b * shade);
    mesh.texcoords.push_back(0.0f);
    mesh.texcoords.push_back(0.0f);
}

void appendFace(MeshData& mesh, int face, float x, float y, float z, const Color3& color) {
    static constexpr float kShades[6] = {0.82f, 0.82f, 1.00f, 0.55f, 0.72f, 0.90f};
    const float shade = kShades[face];

    switch (face) {
        case 0:
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x, y, z + 1.0f, color, shade);
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x, y + 1.0f, z, color, shade);
            break;
        case 1:
            pushVertex(mesh, x + 1.0f, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y, z + 1.0f, color, shade);
            break;
        case 2:
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z, color, shade);
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z, color, shade);
            pushVertex(mesh, x, y + 1.0f, z, color, shade);
            break;
        case 3:
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y, z + 1.0f, color, shade);
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y, z + 1.0f, color, shade);
            pushVertex(mesh, x, y, z + 1.0f, color, shade);
            break;
        case 4:
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x, y + 1.0f, z, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z, color, shade);
            pushVertex(mesh, x, y, z, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z, color, shade);
            pushVertex(mesh, x + 1.0f, y, z, color, shade);
            break;
        case 5:
            pushVertex(mesh, x + 1.0f, y, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x + 1.0f, y, z + 1.0f, color, shade);
            pushVertex(mesh, x, y + 1.0f, z + 1.0f, color, shade);
            pushVertex(mesh, x, y, z + 1.0f, color, shade);
            break;
        default:
            break;
    }
}

}  // namespace

World::World(WorldConfig config)
    : config_(std::move(config)), persistence_(config_.saveRoot, config_.worldName) {
    WorldMeta meta;
    if (persistence_.loadMeta(meta) && meta.seed != 0) {
        seed_ = meta.seed;
        if (meta.hasPlayerState) {
            savedPlayerState_ = meta.player;
        }
        if (meta.hasInventory) {
            savedInventory_ = meta.inventory;
        }
    } else {
        seed_ = config_.seed == 0 ? 13371337U : config_.seed;
        meta.seed = seed_;
        (void)persistence_.saveMeta(meta);
    }
}

std::optional<PlayerState> World::initialPlayerState() const {
    return savedPlayerState_;
}

std::optional<InventoryState> World::initialInventory() const {
    return savedInventory_;
}

void World::storePlayerState(const PlayerState& player) {
    savedPlayerState_ = player;
    metaDirty_ = true;
}

void World::storeInventory(const InventoryState& inventory) {
    savedInventory_ = inventory;
    metaDirty_ = true;
}

int World::terrainHeight(int worldX, int worldZ) const {
    const float lowFreq = fractalNoise(worldX * 0.035f, worldZ * 0.035f, seed_ ^ 0x1a2b3c4dU);
    const float midFreq = fractalNoise(worldX * 0.085f, worldZ * 0.085f, seed_ ^ 0x77889911U);
    const float detail = fractalNoise(worldX * 0.18f, worldZ * 0.18f, seed_ ^ 0x00bada55U);
    const float height = 11.0f + lowFreq * 22.0f + midFreq * 10.0f + detail * 4.0f;
    return std::clamp(static_cast<int>(std::round(height)), 6, kChunkSizeY - 6);
}

bool World::treeAt(int worldX, int worldZ) const {
    const int surface = terrainHeight(worldX, worldZ);
    if (surface < 18) {
        return false;
    }

    const std::uint32_t hash = hash2D(worldX, worldZ, seed_ ^ 0x51f15e77U);
    if ((hash & 31U) != 0U) {
        return false;
    }
    return valueNoise(worldX * 0.06f, worldZ * 0.06f, seed_ ^ 0xa53bd91fU) > 0.42f;
}

BlockId World::generatedBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0) {
        return BlockId::Stone;
    }
    if (worldY >= kChunkSizeY) {
        return BlockId::Air;
    }

    const int surface = terrainHeight(worldX, worldZ);
    const bool sandy = surface <= 16;

    if (worldY <= surface) {
        if (sandy) {
            return worldY >= surface - 3 ? BlockId::Sand : BlockId::Stone;
        }
        if (worldY == surface) {
            return BlockId::Grass;
        }
        if (worldY >= surface - 3) {
            return BlockId::Dirt;
        }
        return BlockId::Stone;
    }

    for (int treeX = worldX - 2; treeX <= worldX + 2; ++treeX) {
        for (int treeZ = worldZ - 2; treeZ <= worldZ + 2; ++treeZ) {
            if (!treeAt(treeX, treeZ)) {
                continue;
            }

            const int trunkBase = terrainHeight(treeX, treeZ) + 1;
            const int trunkHeight = 4 + static_cast<int>(hash2D(treeX, treeZ, seed_ ^ 0x3c6ef372U) & 1U);
            const int crownCenter = trunkBase + trunkHeight - 1;
            if (worldX == treeX && worldZ == treeZ && worldY >= trunkBase && worldY < trunkBase + trunkHeight) {
                return BlockId::Wood;
            }

            const int dx = std::abs(worldX - treeX);
            const int dz = std::abs(worldZ - treeZ);
            const int dy = worldY - crownCenter;
            if (std::abs(dy) > 1 || dx > 2 || dz > 2) {
                continue;
            }

            const int spread = dx + dz + std::abs(dy);
            const bool topCap = dy == 1 && (dx + dz) <= 1;
            const bool shell = spread <= 3;
            const bool clearTrunk = !(dx == 0 && dz == 0 && dy < 1);
            if (clearTrunk && (shell || topCap)) {
                return BlockId::Leaves;
            }
        }
    }

    return BlockId::Air;
}

Chunk* World::findChunk(const ChunkCoord& coord) {
    const auto it = loadedChunks_.find(coord);
    return it == loadedChunks_.end() ? nullptr : it->second.get();
}

const Chunk* World::findChunk(const ChunkCoord& coord) const {
    const auto it = loadedChunks_.find(coord);
    return it == loadedChunks_.end() ? nullptr : it->second.get();
}

Chunk& World::ensureChunkLoaded(const ChunkCoord& coord) {
    if (Chunk* existing = findChunk(coord)) {
        return *existing;
    }

    auto chunk = std::make_unique<Chunk>();
    chunk->coord = coord;
    loadChunk(*chunk);
    Chunk& reference = *chunk;
    loadedChunks_.emplace(coord, std::move(chunk));
    markChunkAndNeighborsDirty(coord);
    return reference;
}

void World::loadChunk(Chunk& chunk) {
    if (persistence_.loadChunk(chunk.coord, chunk.blocks)) {
        chunk.modified = false;
        chunk.meshDirty = true;
        return;
    }
    generateChunk(chunk);
}

void World::generateChunk(Chunk& chunk) const {
    for (int y = 0; y < kChunkSizeY; ++y) {
        for (int z = 0; z < kChunkSizeZ; ++z) {
            for (int x = 0; x < kChunkSizeX; ++x) {
                const int worldX = chunk.coord.x * kChunkSizeX + x;
                const int worldZ = chunk.coord.z * kChunkSizeZ + z;
                chunk.set(x, y, z, generatedBlock(worldX, y, worldZ));
            }
        }
    }
    chunk.modified = false;
    chunk.meshDirty = true;
}

void World::warmStart(const ChunkCoord& center, int radius) {
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            ensureChunkLoaded({center.x + dx, center.z + dz});
        }
    }

    for (auto& [coord, chunk] : loadedChunks_) {
        if (chunkInRadius(center, coord, radius + 1)) {
            rebuildChunkMesh(*chunk);
        }
    }
}

void World::updateStreaming(const ChunkCoord& center) {
    unloadFarChunks(center);
    if (loadOneMissingChunk(center)) {
        return;
    }
    (void)rebuildOneDirtyChunk(center);
}

std::vector<const Chunk*> World::collectRenderable(const ChunkCoord& center) const {
    std::vector<const Chunk*> renderables;
    for (const auto& [coord, chunk] : loadedChunks_) {
        if (!chunkInRadius(center, coord, config_.renderRadius)) {
            continue;
        }
        if (!chunk->mesh.empty()) {
            renderables.push_back(chunk.get());
        }
    }

    std::sort(renderables.begin(), renderables.end(),
              [&center](const Chunk* lhs, const Chunk* rhs) {
                  return chunkDistanceSquared(lhs->coord, center) < chunkDistanceSquared(rhs->coord, center);
              });
    return renderables;
}

BlockId World::getBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0) {
        return BlockId::Stone;
    }
    if (worldY >= kChunkSizeY) {
        return BlockId::Air;
    }

    const ChunkCoord coord = worldToChunkCoord(worldX, worldZ);
    if (const Chunk* chunk = findChunk(coord)) {
        return chunk->get(worldToLocalX(worldX), worldY, worldToLocalZ(worldZ));
    }
    return generatedBlock(worldX, worldY, worldZ);
}

bool World::setBlock(int worldX, int worldY, int worldZ, BlockId block) {
    if (worldY < 0 || worldY >= kChunkSizeY) {
        return false;
    }

    const ChunkCoord coord = worldToChunkCoord(worldX, worldZ);
    Chunk& chunk = ensureChunkLoaded(coord);
    const int localX = worldToLocalX(worldX);
    const int localZ = worldToLocalZ(worldZ);
    if (chunk.get(localX, worldY, localZ) == block) {
        return false;
    }

    chunk.set(localX, worldY, localZ, block);
    chunk.modified = true;
    chunk.meshDirty = true;
    markChunkAndNeighborsDirty(coord);
    (void)persistence_.saveChunk(coord, chunk.blocks);
    chunk.modified = false;
    return true;
}

Vec3 World::spawnPosition() const {
    const int height = terrainHeight(0, 0);
    return {0.5f, static_cast<float>(height) + 1.05f, 0.5f};
}

bool World::collidesAabb(const Vec3& min, const Vec3& max) const {
    const int startX = static_cast<int>(std::floor(min.x));
    const int endX = static_cast<int>(std::floor(max.x - 0.0001f));
    const int startY = static_cast<int>(std::floor(min.y));
    const int endY = static_cast<int>(std::floor(max.y - 0.0001f));
    const int startZ = static_cast<int>(std::floor(min.z));
    const int endZ = static_cast<int>(std::floor(max.z - 0.0001f));

    for (int y = startY; y <= endY; ++y) {
        for (int z = startZ; z <= endZ; ++z) {
            for (int x = startX; x <= endX; ++x) {
                if (isSolid(getBlock(x, y, z))) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::optional<RaycastHit> World::raycast(const Vec3& origin, const Vec3& direction, float maxDistance) const {
    const Vec3 ray = normalize(direction);
    if (length(ray) <= 0.0001f) {
        return std::nullopt;
    }

    IVec3 cell = floorToBlock(origin);
    IVec3 previous = cell;
    const int stepX = ray.x > 0.0f ? 1 : (ray.x < 0.0f ? -1 : 0);
    const int stepY = ray.y > 0.0f ? 1 : (ray.y < 0.0f ? -1 : 0);
    const int stepZ = ray.z > 0.0f ? 1 : (ray.z < 0.0f ? -1 : 0);

    const float inf = std::numeric_limits<float>::infinity();
    const float nextBoundaryX = stepX > 0 ? static_cast<float>(cell.x + 1) : static_cast<float>(cell.x);
    const float nextBoundaryY = stepY > 0 ? static_cast<float>(cell.y + 1) : static_cast<float>(cell.y);
    const float nextBoundaryZ = stepZ > 0 ? static_cast<float>(cell.z + 1) : static_cast<float>(cell.z);

    float tMaxX = stepX != 0 ? (nextBoundaryX - origin.x) / ray.x : inf;
    float tMaxY = stepY != 0 ? (nextBoundaryY - origin.y) / ray.y : inf;
    float tMaxZ = stepZ != 0 ? (nextBoundaryZ - origin.z) / ray.z : inf;
    if (tMaxX < 0.0f) {
        tMaxX = 0.0f;
    }
    if (tMaxY < 0.0f) {
        tMaxY = 0.0f;
    }
    if (tMaxZ < 0.0f) {
        tMaxZ = 0.0f;
    }

    const float tDeltaX = stepX != 0 ? std::abs(1.0f / ray.x) : inf;
    const float tDeltaY = stepY != 0 ? std::abs(1.0f / ray.y) : inf;
    const float tDeltaZ = stepZ != 0 ? std::abs(1.0f / ray.z) : inf;

    float traveled = 0.0f;
    for (;;) {
        const BlockId block = getBlock(cell.x, cell.y, cell.z);
        if (isSolid(block)) {
            return RaycastHit {cell, previous, block, traveled};
        }

        previous = cell;
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            cell.x += stepX;
            traveled = tMaxX;
            tMaxX += tDeltaX;
        } else if (tMaxY < tMaxZ) {
            cell.y += stepY;
            traveled = tMaxY;
            tMaxY += tDeltaY;
        } else {
            cell.z += stepZ;
            traveled = tMaxZ;
            tMaxZ += tDeltaZ;
        }

        if (traveled > maxDistance) {
            break;
        }
    }

    return std::nullopt;
}

void World::flushAll() {
    for (const auto& [coord, chunk] : loadedChunks_) {
        if (chunk->modified) {
            (void)persistence_.saveChunk(coord, chunk->blocks);
        }
    }

    if (metaDirty_ || savedPlayerState_.has_value() || savedInventory_.has_value()) {
        WorldMeta meta;
        meta.seed = seed_;
        meta.hasPlayerState = savedPlayerState_.has_value();
        if (savedPlayerState_.has_value()) {
            meta.player = *savedPlayerState_;
        }
        meta.hasInventory = savedInventory_.has_value();
        if (savedInventory_.has_value()) {
            meta.inventory = *savedInventory_;
        }
        if (persistence_.saveMeta(meta)) {
            metaDirty_ = false;
        }
    }
}

void World::rebuildChunkMesh(Chunk& chunk) {
    chunk.mesh.clear();
    chunk.mesh.vertices.reserve(4096);
    chunk.mesh.colors.reserve(4096);
    chunk.mesh.texcoords.reserve(4096);

    for (int y = 0; y < kChunkSizeY; ++y) {
        for (int z = 0; z < kChunkSizeZ; ++z) {
            for (int x = 0; x < kChunkSizeX; ++x) {
                const BlockId block = chunk.get(x, y, z);
                if (block == BlockId::Air) {
                    continue;
                }

                const int worldX = chunk.coord.x * kChunkSizeX + x;
                const int worldZ = chunk.coord.z * kChunkSizeZ + z;
                const float fx = static_cast<float>(worldX);
                const float fy = static_cast<float>(y);
                const float fz = static_cast<float>(worldZ);
                const Color3 color = blockColor(block);

                if (!isSolid(getBlock(worldX - 1, y, worldZ))) {
                    appendFace(chunk.mesh, 0, fx, fy, fz, color);
                }
                if (!isSolid(getBlock(worldX + 1, y, worldZ))) {
                    appendFace(chunk.mesh, 1, fx, fy, fz, color);
                }
                if (!isSolid(getBlock(worldX, y + 1, worldZ))) {
                    appendFace(chunk.mesh, 2, fx, fy, fz, color);
                }
                if (!isSolid(getBlock(worldX, y - 1, worldZ))) {
                    appendFace(chunk.mesh, 3, fx, fy, fz, color);
                }
                if (!isSolid(getBlock(worldX, y, worldZ - 1))) {
                    appendFace(chunk.mesh, 4, fx, fy, fz, color);
                }
                if (!isSolid(getBlock(worldX, y, worldZ + 1))) {
                    appendFace(chunk.mesh, 5, fx, fy, fz, color);
                }
            }
        }
    }

    chunk.meshDirty = false;
}

void World::markChunkAndNeighborsDirty(const ChunkCoord& coord) {
    static constexpr ChunkCoord neighbors[] = {
        {0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1},
    };

    for (const ChunkCoord offset : neighbors) {
        if (Chunk* chunk = findChunk({coord.x + offset.x, coord.z + offset.z})) {
            chunk->meshDirty = true;
        }
    }
}

void World::unloadFarChunks(const ChunkCoord& center) {
    for (auto it = loadedChunks_.begin(); it != loadedChunks_.end();) {
        if (chunkInRadius(center, it->first, config_.activeRadius)) {
            ++it;
            continue;
        }
        if (it->second->modified) {
            (void)persistence_.saveChunk(it->first, it->second->blocks);
        }
        it = loadedChunks_.erase(it);
    }
}

bool World::loadOneMissingChunk(const ChunkCoord& center) {
    std::optional<ChunkCoord> best;
    int bestDistance = std::numeric_limits<int>::max();

    for (int dz = -config_.activeRadius; dz <= config_.activeRadius; ++dz) {
        for (int dx = -config_.activeRadius; dx <= config_.activeRadius; ++dx) {
            const ChunkCoord candidate {center.x + dx, center.z + dz};
            if (findChunk(candidate) != nullptr) {
                continue;
            }

            const int distance = chunkDistanceSquared(center, candidate);
            if (distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
    }

    if (!best.has_value()) {
        return false;
    }

    ensureChunkLoaded(*best);
    return true;
}

bool World::rebuildOneDirtyChunk(const ChunkCoord& center) {
    Chunk* best = nullptr;
    int bestDistance = std::numeric_limits<int>::max();

    for (auto& [coord, chunk] : loadedChunks_) {
        if (!chunk->meshDirty) {
            continue;
        }

        const int distance = chunkDistanceSquared(center, coord);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = chunk.get();
        }
    }

    if (best == nullptr) {
        return false;
    }

    rebuildChunkMesh(*best);
    return true;
}

}  // namespace voxel
