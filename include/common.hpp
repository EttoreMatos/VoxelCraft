#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace voxel {

constexpr int kChunkSizeX = 16;
constexpr int kChunkSizeY = 64;
constexpr int kChunkSizeZ = 16;
constexpr int kChunkVolume = kChunkSizeX * kChunkSizeY * kChunkSizeZ;
constexpr int kActiveRadius = 4;
constexpr int kRenderRadius = 3;
constexpr int kAtlasGridSize = 4;
constexpr float kPlayerRadius = 0.35f;
constexpr float kPlayerHeight = 1.8f;
constexpr float kCameraHeight = 1.62f;
constexpr float kPi = 3.14159265358979323846f;

enum class BlockId : std::uint8_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    Sand = 4,
    Wood = 5,
    Leaves = 6,
    Brick = 7,
    Cobblestone = 8,
    Planks = 9,
    Gravel = 10,
    Snow = 11,
};

static_assert(sizeof(BlockId) == 1, "BlockId must stay byte-sized for saves.");

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
};

inline Vec3 operator+(Vec3 lhs, const Vec3& rhs) {
    lhs += rhs;
    return lhs;
}

inline Vec3 operator-(Vec3 lhs, const Vec3& rhs) {
    lhs -= rhs;
    return lhs;
}

inline Vec3 operator*(Vec3 value, float scalar) {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

inline Vec3 operator*(float scalar, Vec3 value) {
    return value * scalar;
}

inline Vec3 operator/(Vec3 value, float scalar) {
    return {value.x / scalar, value.y / scalar, value.z / scalar};
}

struct IVec3 {
    int x = 0;
    int y = 0;
    int z = 0;

    auto operator<=>(const IVec3&) const = default;
};

struct ChunkCoord {
    int x = 0;
    int z = 0;

    auto operator<=>(const ChunkCoord&) const = default;
};

struct Color3 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct UVRect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct MeshData {
    std::vector<float> vertices;
    std::vector<float> colors;
    std::vector<float> texcoords;

    void clear() {
        vertices.clear();
        colors.clear();
        texcoords.clear();
    }

    [[nodiscard]] bool empty() const {
        return vertices.empty();
    }

    [[nodiscard]] std::size_t vertexCount() const {
        return vertices.size() / 3;
    }
};

struct PlayerState {
    Vec3 position;
    Vec3 velocity;
    float yawDegrees = 180.0f;
    float pitchDegrees = 0.0f;
    bool flying = false;
    bool onGround = false;
};

struct RaycastHit {
    IVec3 block;
    IVec3 previousEmpty;
    BlockId blockId = BlockId::Air;
    float distance = 0.0f;
};

inline int floorDiv(int value, int divisor) {
    int quotient = value / divisor;
    int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

inline int positiveMod(int value, int divisor) {
    int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

inline ChunkCoord worldToChunkCoord(int worldX, int worldZ) {
    return {floorDiv(worldX, kChunkSizeX), floorDiv(worldZ, kChunkSizeZ)};
}

inline int worldToLocalX(int worldX) {
    return positiveMod(worldX, kChunkSizeX);
}

inline int worldToLocalZ(int worldZ) {
    return positiveMod(worldZ, kChunkSizeZ);
}

inline IVec3 floorToBlock(const Vec3& value) {
    return {
        static_cast<int>(std::floor(value.x)),
        static_cast<int>(std::floor(value.y)),
        static_cast<int>(std::floor(value.z)),
    };
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

inline Vec3 normalize(const Vec3& value) {
    const float len = length(value);
    if (len <= 0.00001f) {
        return {};
    }
    return value / len;
}

inline float clampf(float value, float low, float high) {
    return std::clamp(value, low, high);
}

inline bool isSolid(BlockId block) {
    return block != BlockId::Air;
}

inline Color3 blockColor(BlockId block) {
    switch (block) {
        case BlockId::Grass:
            return {0.29f, 0.72f, 0.27f};
        case BlockId::Dirt:
            return {0.47f, 0.32f, 0.18f};
        case BlockId::Stone:
            return {0.58f, 0.58f, 0.62f};
        case BlockId::Sand:
            return {0.84f, 0.78f, 0.50f};
        case BlockId::Wood:
            return {0.52f, 0.36f, 0.19f};
        case BlockId::Leaves:
            return {0.22f, 0.48f, 0.18f};
        case BlockId::Brick:
            return {0.70f, 0.22f, 0.20f};
        case BlockId::Cobblestone:
            return {0.47f, 0.47f, 0.49f};
        case BlockId::Planks:
            return {0.71f, 0.56f, 0.31f};
        case BlockId::Gravel:
            return {0.54f, 0.54f, 0.52f};
        case BlockId::Snow:
            return {0.93f, 0.95f, 0.97f};
        case BlockId::Air:
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

inline int atlasTileIndexForFace(BlockId block, int face) {
    switch (block) {
        case BlockId::Grass:
            if (face == 2) {
                return 0;
            }
            if (face == 3) {
                return 2;
            }
            return 1;
        case BlockId::Dirt:
            return 2;
        case BlockId::Stone:
            return 3;
        case BlockId::Sand:
            return 4;
        case BlockId::Wood:
            return (face == 2 || face == 3) ? 6 : 5;
        case BlockId::Leaves:
            return 7;
        case BlockId::Brick:
            return 8;
        case BlockId::Cobblestone:
            return 9;
        case BlockId::Planks:
            return 10;
        case BlockId::Gravel:
            return 11;
        case BlockId::Snow:
            return 12;
        case BlockId::Air:
        default:
            return 2;
    }
}

inline int atlasTileIndexForIcon(BlockId block) {
    switch (block) {
        case BlockId::Grass:
            return 1;
        case BlockId::Dirt:
            return 2;
        case BlockId::Stone:
            return 3;
        case BlockId::Sand:
            return 4;
        case BlockId::Wood:
            return 5;
        case BlockId::Leaves:
            return 7;
        case BlockId::Brick:
            return 8;
        case BlockId::Cobblestone:
            return 9;
        case BlockId::Planks:
            return 10;
        case BlockId::Gravel:
            return 11;
        case BlockId::Snow:
            return 12;
        case BlockId::Air:
        default:
            return 2;
    }
}

inline UVRect atlasUvForTile(int tileIndex) {
    constexpr float cell = 1.0f / static_cast<float>(kAtlasGridSize);
    constexpr float margin = 0.5f / 64.0f;
    const int col = tileIndex % kAtlasGridSize;
    const int row = tileIndex / kAtlasGridSize;
    return {
        col * cell + margin,
        row * cell + margin,
        (col + 1) * cell - margin,
        (row + 1) * cell - margin,
    };
}

inline UVRect blockFaceUv(BlockId block, int face) {
    return atlasUvForTile(atlasTileIndexForFace(block, face));
}

inline UVRect blockIconUv(BlockId block) {
    return atlasUvForTile(atlasTileIndexForIcon(block));
}

inline const std::array<BlockId, 11>& creativeBlockCatalog() {
    static const std::array<BlockId, 11> catalog = {
        BlockId::Grass,
        BlockId::Dirt,
        BlockId::Stone,
        BlockId::Sand,
        BlockId::Wood,
        BlockId::Leaves,
        BlockId::Brick,
        BlockId::Cobblestone,
        BlockId::Planks,
        BlockId::Gravel,
        BlockId::Snow,
    };
    return catalog;
}

}  // namespace voxel
