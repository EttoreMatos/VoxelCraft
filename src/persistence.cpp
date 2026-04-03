#include "persistence.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace voxel {
namespace {

std::string sanitizeWorldName(std::string name) {
    for (char& ch : name) {
        const bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        if (!valid) {
            ch = '_';
        }
    }
    if (name.empty()) {
        return "default";
    }
    return name;
}

template <typename T>
bool readValue(std::istream& input, T& value) {
    input.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return static_cast<bool>(input);
}

template <typename T>
bool writeValue(std::ostream& output, const T& value) {
    output.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return static_cast<bool>(output);
}

constexpr char kMetaMagic[4] = {'V', 'X', 'M', '2'};
constexpr char kChunkMagic[4] = {'V', 'X', 'C', '2'};

}  // namespace

WorldPersistence::WorldPersistence(std::filesystem::path saveRoot, std::string worldName)
    : worldName_(sanitizeWorldName(std::move(worldName))), saveRoot_(std::move(saveRoot)) {
    savePath_ = saveRoot_ / worldName_;
    chunkPath_ = savePath_ / "chunks";
    metaPath_ = savePath_ / "meta.bin";
    std::filesystem::create_directories(chunkPath_);
}

bool WorldPersistence::loadMeta(WorldMeta& meta) const {
    if (!std::filesystem::exists(metaPath_)) {
        return false;
    }

    std::ifstream input(metaPath_, std::ios::binary);
    if (!input) {
        return false;
    }

    char magic[4] {};
    std::uint32_t version = 0;
    std::uint32_t seed = 0;
    std::uint8_t hasPlayer = 0;
    if (!input.read(magic, sizeof(magic)) || !readValue(input, version) || !readValue(input, seed) ||
        !readValue(input, hasPlayer)) {
        return false;
    }

    if (std::string_view(magic, sizeof(magic)) != std::string_view(kMetaMagic, sizeof(kMetaMagic)) ||
        version != kSaveVersion) {
        return false;
    }

    meta.seed = seed;
    meta.hasPlayerState = hasPlayer != 0;
    if (!meta.hasPlayerState) {
        return true;
    }

    std::uint8_t flags = 0;
    return readValue(input, meta.player.position.x) && readValue(input, meta.player.position.y) &&
           readValue(input, meta.player.position.z) && readValue(input, meta.player.velocity.x) &&
           readValue(input, meta.player.velocity.y) && readValue(input, meta.player.velocity.z) &&
           readValue(input, meta.player.yawDegrees) && readValue(input, meta.player.pitchDegrees) &&
           readValue(input, flags) &&
           ((meta.player.flying = (flags & 0x1U) != 0U), (meta.player.onGround = (flags & 0x2U) != 0U), true);
}

bool WorldPersistence::saveMeta(const WorldMeta& meta) const {
    std::filesystem::create_directories(savePath_);
    std::ofstream output(metaPath_, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    const std::uint32_t version = kSaveVersion;
    const std::uint8_t hasPlayer = meta.hasPlayerState ? 1U : 0U;
    const std::uint8_t flags = (meta.player.flying ? 0x1U : 0U) | (meta.player.onGround ? 0x2U : 0U);

    output.write(kMetaMagic, sizeof(kMetaMagic));
    return writeValue(output, version) && writeValue(output, meta.seed) && writeValue(output, hasPlayer) &&
           (!meta.hasPlayerState ||
            (writeValue(output, meta.player.position.x) && writeValue(output, meta.player.position.y) &&
             writeValue(output, meta.player.position.z) && writeValue(output, meta.player.velocity.x) &&
             writeValue(output, meta.player.velocity.y) && writeValue(output, meta.player.velocity.z) &&
             writeValue(output, meta.player.yawDegrees) && writeValue(output, meta.player.pitchDegrees) &&
             writeValue(output, flags)));
}

bool WorldPersistence::loadChunk(const ChunkCoord& coord, std::array<BlockId, kChunkVolume>& blocks) const {
    const auto path = chunkFilePath(coord);
    if (!std::filesystem::exists(path)) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    char magic[4] {};
    std::uint32_t version = 0;
    std::int32_t x = 0;
    std::int32_t z = 0;
    input.read(magic, sizeof(magic));
    if (!readValue(input, version) || !readValue(input, x) || !readValue(input, z)) {
        return false;
    }
    input.read(reinterpret_cast<char*>(blocks.data()), static_cast<std::streamsize>(blocks.size() * sizeof(BlockId)));

    return static_cast<bool>(input) &&
           std::string_view(magic, sizeof(magic)) == std::string_view(kChunkMagic, sizeof(kChunkMagic)) &&
           version == kSaveVersion && x == coord.x && z == coord.z;
}

bool WorldPersistence::saveChunk(const ChunkCoord& coord, const std::array<BlockId, kChunkVolume>& blocks) const {
    std::filesystem::create_directories(chunkPath_);
    std::ofstream output(chunkFilePath(coord), std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    const std::uint32_t version = kSaveVersion;
    const std::int32_t x = coord.x;
    const std::int32_t z = coord.z;

    output.write(kChunkMagic, sizeof(kChunkMagic));
    output.write(reinterpret_cast<const char*>(&version), static_cast<std::streamsize>(sizeof(version)));
    output.write(reinterpret_cast<const char*>(&x), static_cast<std::streamsize>(sizeof(x)));
    output.write(reinterpret_cast<const char*>(&z), static_cast<std::streamsize>(sizeof(z)));
    output.write(reinterpret_cast<const char*>(blocks.data()),
                 static_cast<std::streamsize>(blocks.size() * sizeof(BlockId)));
    return static_cast<bool>(output);
}

std::filesystem::path WorldPersistence::chunkFilePath(const ChunkCoord& coord) const {
    return chunkPath_ / ("c_" + std::to_string(coord.x) + "_" + std::to_string(coord.z) + ".bin");
}

}  // namespace voxel
