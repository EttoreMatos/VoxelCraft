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

bool readPlayerState(std::istream& input, PlayerState& player) {
    std::uint8_t flags = 0;
    return readValue(input, player.position.x) && readValue(input, player.position.y) &&
           readValue(input, player.position.z) && readValue(input, player.velocity.x) &&
           readValue(input, player.velocity.y) && readValue(input, player.velocity.z) &&
           readValue(input, player.yawDegrees) && readValue(input, player.pitchDegrees) && readValue(input, flags) &&
           ((player.flying = (flags & 0x1U) != 0U), (player.onGround = (flags & 0x2U) != 0U), true);
}

bool writePlayerState(std::ostream& output, const PlayerState& player) {
    const std::uint8_t flags = (player.flying ? 0x1U : 0U) | (player.onGround ? 0x2U : 0U);
    return writeValue(output, player.position.x) && writeValue(output, player.position.y) &&
           writeValue(output, player.position.z) && writeValue(output, player.velocity.x) &&
           writeValue(output, player.velocity.y) && writeValue(output, player.velocity.z) &&
           writeValue(output, player.yawDegrees) && writeValue(output, player.pitchDegrees) &&
           writeValue(output, flags);
}

bool readInventoryState(std::istream& input, InventoryState& inventory) {
    std::uint8_t selectedHotbar = 0;
    if (!readValue(input, selectedHotbar)) {
        return false;
    }

    inventory = {};
    inventory.selectedHotbarSlot =
        static_cast<std::uint8_t>(std::min<int>(selectedHotbar, kHotbarSlotCount - 1));

    for (ItemStack& slot : inventory.slots) {
        std::uint8_t block = 0;
        std::uint8_t count = 0;
        if (!readValue(input, block) || !readValue(input, count)) {
            return false;
        }

        if (count == 0 || block == static_cast<std::uint8_t>(BlockId::Air) ||
            block > static_cast<std::uint8_t>(BlockId::Snow)) {
            slot.clear();
            continue;
        }

        slot.block = static_cast<BlockId>(block);
        slot.count = static_cast<std::uint8_t>(std::min<int>(count, kMaxStackCount));
    }
    return true;
}

bool writeInventoryState(std::ostream& output, const InventoryState& inventory) {
    const std::uint8_t selectedHotbar =
        static_cast<std::uint8_t>(std::min<int>(inventory.selectedHotbarSlot, kHotbarSlotCount - 1));
    if (!writeValue(output, selectedHotbar)) {
        return false;
    }

    for (ItemStack slot : inventory.slots) {
        if (slot.empty()) {
            slot.clear();
        }

        const std::uint8_t block = static_cast<std::uint8_t>(slot.empty() ? BlockId::Air : slot.block);
        const std::uint8_t count = static_cast<std::uint8_t>(slot.empty() ? 0 : std::min<int>(slot.count, kMaxStackCount));
        if (!writeValue(output, block) || !writeValue(output, count)) {
            return false;
        }
    }
    return true;
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
        (version != 2U && version != kMetaVersion)) {
        return false;
    }

    meta.seed = seed;
    meta.hasPlayerState = hasPlayer != 0;
    meta.hasInventory = false;
    meta.inventory = {};

    if (meta.hasPlayerState && !readPlayerState(input, meta.player)) {
        return false;
    }

    if (version < 3U) {
        return true;
    }

    std::uint8_t hasInventory = 0;
    if (!readValue(input, hasInventory)) {
        return false;
    }
    meta.hasInventory = hasInventory != 0;
    return !meta.hasInventory || readInventoryState(input, meta.inventory);
}

bool WorldPersistence::saveMeta(const WorldMeta& meta) const {
    std::filesystem::create_directories(savePath_);
    std::ofstream output(metaPath_, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    const std::uint32_t version = kMetaVersion;
    const std::uint8_t hasPlayer = meta.hasPlayerState ? 1U : 0U;
    const std::uint8_t hasInventory = meta.hasInventory ? 1U : 0U;

    output.write(kMetaMagic, sizeof(kMetaMagic));
    return writeValue(output, version) && writeValue(output, meta.seed) && writeValue(output, hasPlayer) &&
           (!meta.hasPlayerState || writePlayerState(output, meta.player)) && writeValue(output, hasInventory) &&
           (!meta.hasInventory || writeInventoryState(output, meta.inventory));
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
           version == kChunkVersion && x == coord.x && z == coord.z;
}

bool WorldPersistence::saveChunk(const ChunkCoord& coord, const std::array<BlockId, kChunkVolume>& blocks) const {
    std::filesystem::create_directories(chunkPath_);
    std::ofstream output(chunkFilePath(coord), std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    const std::uint32_t version = kChunkVersion;
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
