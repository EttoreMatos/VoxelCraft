#include "session.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int gFailures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++gFailures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::filesystem::path makeTempRoot() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("voxelcraft_test_" + std::to_string(stamp));
}

void testChunkIndexing() {
    using namespace voxel;
    check(Chunk::index(0, 0, 0) == 0, "chunk index origin");
    check(Chunk::index(15, 0, 0) == 15, "chunk index x edge");
    check(Chunk::index(0, 1, 0) == 256, "chunk index y stride");
    check(Chunk::index(15, 63, 15) == kChunkVolume - 1, "chunk index last block");
}

void testNegativeWorldCoordinates() {
    using namespace voxel;
    check(worldToChunkCoord(-1, -1) == ChunkCoord {-1, -1}, "negative coord one block left");
    check(worldToChunkCoord(-16, -16) == ChunkCoord {-1, -1}, "negative coord exact chunk edge");
    check(worldToChunkCoord(-17, -17) == ChunkCoord {-2, -2}, "negative coord beyond chunk edge");
    check(worldToLocalX(-1) == 15, "negative local x wraps");
    check(worldToLocalZ(-17) == 15, "negative local z wraps");
}

void testDeterministicGeneration(const std::filesystem::path& root) {
    voxel::World first({.worldName = "deterministic", .seed = 424242U, .saveRoot = root});
    voxel::World second({.worldName = "deterministic_copy", .seed = 424242U, .saveRoot = root});

    for (int z = -20; z <= 20; z += 4) {
        for (int x = -20; x <= 20; x += 4) {
            for (int y = 0; y < voxel::kChunkSizeY; y += 8) {
                check(first.getBlock(x, y, z) == second.getBlock(x, y, z),
                      "deterministic generation at sample point");
            }
        }
    }
}

void testSaveLoadRoundTrip(const std::filesystem::path& root) {
    {
        voxel::World world({.worldName = "save_roundtrip", .seed = 777U, .saveRoot = root});
        world.warmStart({0, 0}, 0);
        check(world.setBlock(2, 20, 2, voxel::BlockId::Brick), "placing a brick changes the world");

        voxel::PlayerState player;
        player.position = {4.5f, 21.0f, 3.5f};
        player.yawDegrees = 45.0f;
        player.pitchDegrees = -12.0f;
        player.flying = true;
        world.storePlayerState(player);
        world.flushAll();
    }

    {
        voxel::World reloaded({.worldName = "save_roundtrip", .seed = 123U, .saveRoot = root});
        reloaded.warmStart({0, 0}, 0);
        check(reloaded.getBlock(2, 20, 2) == voxel::BlockId::Brick, "saved block reloads from disk");
        const auto savedPlayer = reloaded.initialPlayerState();
        check(savedPlayer.has_value(), "player state reloads from meta");
        if (savedPlayer.has_value()) {
            check(std::abs(savedPlayer->position.x - 4.5f) < 0.001f, "player x reloads");
            check(savedPlayer->flying, "player flying flag reloads");
        }
    }
}

void testRaycast(const std::filesystem::path& root) {
    voxel::World world({.worldName = "raycast", .seed = 991U, .saveRoot = root});
    world.warmStart({0, 0}, 0);
    world.setBlock(2, 40, 2, voxel::BlockId::Brick);

    const auto hit = world.raycast({2.5f, 43.8f, 2.5f}, {0.0f, -1.0f, 0.0f}, 10.0f);
    check(hit.has_value(), "raycast should hit placed block");
    if (hit.has_value()) {
        check(hit->block == voxel::IVec3 {2, 40, 2}, "raycast hits the correct block");
        check(hit->previousEmpty == voxel::IVec3 {2, 41, 2}, "raycast stores previous empty cell");
    }
}

void testCollision(const std::filesystem::path& root) {
    voxel::World world({.worldName = "collision", .seed = 111U, .saveRoot = root});
    world.warmStart({0, 0}, 0);
    world.setBlock(0, 40, 0, voxel::BlockId::Stone);

    check(world.collidesAabb({0.10f, 40.10f, 0.10f}, {0.90f, 40.90f, 0.90f}),
          "aabb overlapping a block should collide");
    check(!world.collidesAabb({1.10f, 40.10f, 1.10f}, {1.90f, 40.90f, 1.90f}),
          "aabb away from blocks should not collide");
}

void testSessionMovementAndInventory(const std::filesystem::path& root) {
    voxel::GameSession session({.worldName = "session", .seed = 555U, .saveRoot = root});
    session.initialize();

    const auto before = session.player().position;
    voxel::InputSnapshot moveForward;
    moveForward.forward = true;
    session.update(0.20f, moveForward);
    const auto after = session.player().position;
    check(std::abs(after.x - before.x) > 0.001f || std::abs(after.z - before.z) > 0.001f,
          "session moves the player");

    voxel::InputSnapshot chooseSlot;
    chooseSlot.hotbarSelection = 3;
    session.update(0.016f, chooseSlot);
    check(session.selectedSlot() == 2, "numeric hotbar input updates selected slot");

    voxel::InputSnapshot openInventory;
    openInventory.toggleInventoryPressed = true;
    session.update(0.016f, openInventory);
    check(session.inventoryOpen(), "inventory toggles open");

    voxel::InputSnapshot moveCursor;
    moveCursor.inventoryRightPressed = true;
    session.update(0.016f, moveCursor);

    voxel::InputSnapshot pickBlock;
    pickBlock.inventorySelectPressed = true;
    session.update(0.016f, pickBlock);
    check(session.hotbar()[2] == voxel::BlockId::Dirt, "inventory selection writes into selected hotbar slot");

    session.shutdown();
}

void testVisualMappings() {
    using namespace voxel;
    check(atlasTileIndexForFace(BlockId::Grass, 2) == 0, "grass top tile mapping");
    check(atlasTileIndexForFace(BlockId::Grass, 0) == 1, "grass side tile mapping");
    check(atlasTileIndexForFace(BlockId::Wood, 2) == 6, "log top tile mapping");
    check(atlasTileIndexForIcon(BlockId::Cobblestone) == 9, "cobblestone icon mapping");
    check(creativeBlockCatalog().size() == 11, "creative inventory catalog size");
}

}  // namespace

int main() {
    try {
        const auto root = makeTempRoot();
        std::filesystem::create_directories(root);

        testChunkIndexing();
        testNegativeWorldCoordinates();
        testDeterministicGeneration(root);
        testSaveLoadRoundTrip(root);
        testRaycast(root);
        testCollision(root);
        testSessionMovementAndInventory(root);
        testVisualMappings();

        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (gFailures != 0) {
        std::cerr << gFailures << " teste(s) falharam.\n";
        return 1;
    }

    std::cout << "Todos os testes passaram.\n";
    return 0;
}
