#include "session.hpp"

#include <algorithm>
#include <cmath>

namespace voxel {
namespace {

float radians(float degrees) {
    return degrees * (kPi / 180.0f);
}

bool rangesOverlap(float a0, float a1, float b0, float b1) {
    return a0 < b1 && b0 < a1;
}

int inventoryRows(int count, int columns) {
    return (count + columns - 1) / columns;
}

}  // namespace

GameSession::GameSession(WorldConfig config) : world_(std::move(config)) {
    hotbar_ = {
        BlockId::Grass,
        BlockId::Dirt,
        BlockId::Stone,
        BlockId::Sand,
        BlockId::Wood,
        BlockId::Leaves,
        BlockId::Brick,
    };
    const auto& catalog = creativeBlockCatalog();
    creativeInventory_.assign(catalog.begin(), catalog.end());
}

void GameSession::initialize() {
    if (const auto saved = world_.initialPlayerState(); saved.has_value()) {
        player_ = *saved;
    } else {
        player_.position = world_.spawnPosition();
        player_.yawDegrees = 225.0f;
        player_.pitchDegrees = -10.0f;
    }

    world_.warmStart(playerChunk(), 1);
    for (int step = 0; step < 12; ++step) {
        world_.updateStreaming(playerChunk());
    }
    currentHit_ = world_.raycast(cameraPosition(), viewDirection(), 6.0f);
}

void GameSession::update(float deltaTime, const InputSnapshot& input) {
    world_.updateStreaming(playerChunk());
    updateView(input);
    updateInventory(input);

    if (inventoryOpen_) {
        currentHit_.reset();
        return;
    }

    updateMovement(deltaTime, input);
    updateInteraction(input);
    currentHit_ = world_.raycast(cameraPosition(), viewDirection(), 6.0f);
}

void GameSession::shutdown() {
    world_.storePlayerState(player_);
    world_.flushAll();
}

ChunkCoord GameSession::playerChunk() const {
    const auto feet = floorToBlock(player_.position);
    return worldToChunkCoord(feet.x, feet.z);
}

Vec3 GameSession::cameraPosition() const {
    return player_.position + Vec3 {0.0f, kCameraHeight, 0.0f};
}

Vec3 GameSession::viewDirection() const {
    const float yaw = radians(player_.yawDegrees);
    const float pitch = radians(player_.pitchDegrees);
    const float cosPitch = std::cos(pitch);
    return normalize({
        -std::sin(yaw) * cosPitch,
        std::sin(pitch),
        -std::cos(yaw) * cosPitch,
    });
}

Vec3 GameSession::playerMin() const {
    return {player_.position.x - kPlayerRadius, player_.position.y, player_.position.z - kPlayerRadius};
}

Vec3 GameSession::playerMax() const {
    return {player_.position.x + kPlayerRadius, player_.position.y + kPlayerHeight,
            player_.position.z + kPlayerRadius};
}

bool GameSession::wouldClipPlayer(const IVec3& block) const {
    const float epsilon = 0.001f;
    const Vec3 pMin = playerMin();
    const Vec3 pMax = playerMax();
    const float bMinX = static_cast<float>(block.x);
    const float bMinY = static_cast<float>(block.y);
    const float bMinZ = static_cast<float>(block.z);
    const float bMaxX = bMinX + 1.0f;
    const float bMaxY = bMinY + 1.0f;
    const float bMaxZ = bMinZ + 1.0f;

    return rangesOverlap(pMin.x + epsilon, pMax.x - epsilon, bMinX, bMaxX) &&
           rangesOverlap(pMin.y + epsilon, pMax.y - epsilon, bMinY, bMaxY) &&
           rangesOverlap(pMin.z + epsilon, pMax.z - epsilon, bMinZ, bMaxZ);
}

void GameSession::updateView(const InputSnapshot& input) {
    if (input.hotbarSelection >= 1 && input.hotbarSelection <= static_cast<int>(hotbar_.size())) {
        selectedSlot_ = input.hotbarSelection - 1;
    }

    if (input.toggleInventoryPressed) {
        inventoryOpen_ = !inventoryOpen_;
        currentHit_.reset();
        return;
    }

    if (inventoryOpen_ || !input.mouseCaptured) {
        return;
    }

    constexpr float kMouseSensitivity = 0.13f;
    player_.yawDegrees -= input.mouseDx * kMouseSensitivity;
    player_.pitchDegrees -= input.mouseDy * kMouseSensitivity;
    player_.pitchDegrees = clampf(player_.pitchDegrees, -89.0f, 89.0f);
    if (player_.yawDegrees >= 360.0f) {
        player_.yawDegrees -= 360.0f;
    } else if (player_.yawDegrees < 0.0f) {
        player_.yawDegrees += 360.0f;
    }

    if (input.toggleFlyPressed) {
        player_.flying = !player_.flying;
        player_.velocity = {};
        player_.onGround = false;
    }
}

void GameSession::updateMovement(float deltaTime, const InputSnapshot& input) {
    const float yaw = radians(player_.yawDegrees);
    const Vec3 forward {-std::sin(yaw), 0.0f, -std::cos(yaw)};
    const Vec3 right {std::cos(yaw), 0.0f, -std::sin(yaw)};

    Vec3 wish {};
    if (input.forward) {
        wish += forward;
    }
    if (input.back) {
        wish -= forward;
    }
    if (input.right) {
        wish += right;
    }
    if (input.left) {
        wish -= right;
    }

    if (length(wish) > 0.0f) {
        wish = normalize(wish);
    }

    if (player_.flying) {
        const float speed = input.sprint ? 10.0f : 7.0f;
        const float vertical = (input.jump ? 1.0f : 0.0f) - (input.descend ? 1.0f : 0.0f);
        const Vec3 motion {wish.x * speed * deltaTime, vertical * speed * deltaTime, wish.z * speed * deltaTime};
        player_.velocity = motion / std::max(deltaTime, 0.0001f);
        moveAndCollide(motion);
        player_.velocity.y = 0.0f;
        return;
    }

    const float speed = input.sprint ? 7.2f : 4.6f;
    player_.velocity.x = wish.x * speed;
    player_.velocity.z = wish.z * speed;
    if (input.jump && player_.onGround) {
        player_.velocity.y = 7.4f;
        player_.onGround = false;
    }

    player_.velocity.y -= 20.0f * deltaTime;
    player_.velocity.y = std::max(player_.velocity.y, -30.0f);
    moveAndCollide(player_.velocity * deltaTime);
}

void GameSession::updateInventory(const InputSnapshot& input) {
    if (!inventoryOpen_) {
        return;
    }

    constexpr int kColumns = 4;
    const int count = static_cast<int>(creativeInventory_.size());
    const int rowCount = inventoryRows(count, kColumns);

    int row = inventoryCursor_ / kColumns;
    int col = inventoryCursor_ % kColumns;
    if (input.inventoryLeftPressed) {
        col = std::max(0, col - 1);
    }
    if (input.inventoryRightPressed) {
        col = std::min(kColumns - 1, col + 1);
    }
    if (input.inventoryUpPressed) {
        row = std::max(0, row - 1);
    }
    if (input.inventoryDownPressed) {
        row = std::min(rowCount - 1, row + 1);
    }

    inventoryCursor_ = std::min(count - 1, row * kColumns + col);
    if ((input.inventorySelectPressed || input.clickPrimaryPressed) && inventoryCursor_ >= 0 &&
        inventoryCursor_ < count) {
        hotbar_[selectedSlot_] = creativeInventory_[inventoryCursor_];
    }
}

void GameSession::updateInteraction(const InputSnapshot& input) {
    currentHit_ = world_.raycast(cameraPosition(), viewDirection(), 6.0f);
    if (!currentHit_.has_value()) {
        return;
    }

    if (input.clickPrimaryPressed) {
        world_.setBlock(currentHit_->block.x, currentHit_->block.y, currentHit_->block.z, BlockId::Air);
    }

    if (input.clickSecondaryPressed) {
        const IVec3 place = currentHit_->previousEmpty;
        if (!wouldClipPlayer(place)) {
            world_.setBlock(place.x, place.y, place.z, hotbar_[selectedSlot_]);
        }
    }
}

void GameSession::moveAndCollide(const Vec3& motion) {
    if (!player_.flying) {
        player_.onGround = false;
    }

    moveAxis(motion.x, 0);
    moveAxis(motion.y, 1);
    moveAxis(motion.z, 2);

    if (!player_.flying) {
        Vec3 probeMin = playerMin();
        Vec3 probeMax = playerMax();
        probeMin.y -= 0.05f;
        probeMax.y -= 0.05f;
        player_.onGround = player_.onGround || world_.collidesAabb(probeMin, probeMax);
    }
}

void GameSession::moveAxis(float amount, int axis) {
    if (std::abs(amount) <= 0.00001f) {
        return;
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(amount) / 0.2f)));
    const float stepAmount = amount / static_cast<float>(steps);

    for (int step = 0; step < steps; ++step) {
        float* component = nullptr;
        if (axis == 0) {
            component = &player_.position.x;
        } else if (axis == 1) {
            component = &player_.position.y;
        } else {
            component = &player_.position.z;
        }

        *component += stepAmount;
        if (!world_.collidesAabb(playerMin(), playerMax())) {
            continue;
        }

        *component -= stepAmount;
        if (axis == 1) {
            if (amount < 0.0f) {
                player_.onGround = true;
            }
            player_.velocity.y = 0.0f;
        } else if (axis == 0) {
            player_.velocity.x = 0.0f;
        } else {
            player_.velocity.z = 0.0f;
        }
        break;
    }
}

}  // namespace voxel
