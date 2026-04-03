#include "session.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace voxel {
namespace {

constexpr float kMouseSensitivity = 0.13f;
constexpr float kDropHalfSize = 0.14f;
constexpr float kDropGravity = 14.0f;
constexpr float kDropPickupRadius = 1.25f;
constexpr float kDropDespawnTime = 90.0f;

float radians(float degrees) {
    return degrees * (kPi / 180.0f);
}

bool rangesOverlap(float a0, float a1, float b0, float b1) {
    return a0 < b1 && b0 < a1;
}

int hotbarInventoryIndex(int hotbarSlot) {
    return kInventoryHotbarOffset + hotbarSlot;
}

bool isHotbarInventoryIndex(int inventoryIndex) {
    return inventoryIndex >= kInventoryHotbarOffset && inventoryIndex < kInventorySlotCount;
}

float distanceSquared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

}  // namespace

GameSession::GameSession(WorldConfig config) : world_(std::move(config)) {}

void GameSession::initialize() {
    if (const auto saved = world_.initialPlayerState(); saved.has_value()) {
        player_ = *saved;
    } else {
        player_.position = world_.spawnPosition();
        player_.yawDegrees = 225.0f;
        player_.pitchDegrees = -10.0f;
    }

    if (const auto savedInventory = world_.initialInventory(); savedInventory.has_value()) {
        inventory_ = *savedInventory;
    } else {
        inventory_ = {};
    }

    selectedSlot_ = std::clamp<int>(inventory_.selectedHotbarSlot, 0, kHotbarSlotCount - 1);
    inventory_.selectedHotbarSlot = static_cast<std::uint8_t>(selectedSlot_);
    inventoryCursor_ = hotbarInventoryIndex(selectedSlot_);
    carriedStack_.clear();
    droppedItems_.clear();
    resetBreakState();

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
        resetBreakState();
        updateDrops(deltaTime);
        tryCollectDrops();
        currentHit_.reset();
        return;
    }

    updateMovement(deltaTime, input);
    updateDrops(deltaTime);
    tryCollectDrops();
    currentHit_ = world_.raycast(cameraPosition(), viewDirection(), 6.0f);
    updateInteraction(deltaTime, input);
    currentHit_ = world_.raycast(cameraPosition(), viewDirection(), 6.0f);
}

void GameSession::shutdown() {
    inventory_.selectedHotbarSlot = static_cast<std::uint8_t>(selectedSlot_);
    returnCarriedStackToInventory();
    world_.storePlayerState(player_);
    world_.storeInventory(inventory_);
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
    if (input.hotbarSelection >= 1 && input.hotbarSelection <= kHotbarSlotCount) {
        selectedSlot_ = input.hotbarSelection - 1;
        inventory_.selectedHotbarSlot = static_cast<std::uint8_t>(selectedSlot_);
        if (inventoryOpen_) {
            inventoryCursor_ = hotbarInventoryIndex(selectedSlot_);
        }
    }

    if (input.toggleInventoryPressed) {
        inventoryOpen_ = !inventoryOpen_;
        inventoryCursor_ = hotbarInventoryIndex(selectedSlot_);
        if (!inventoryOpen_) {
            returnCarriedStackToInventory();
        }
        currentHit_.reset();
        resetBreakState();
        return;
    }

    if (inventoryOpen_ || !input.mouseCaptured) {
        return;
    }

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

    int row = inventoryCursor_ / kInventoryColumns;
    int col = inventoryCursor_ % kInventoryColumns;
    if (input.inventoryLeftPressed) {
        col = std::max(0, col - 1);
    }
    if (input.inventoryRightPressed) {
        col = std::min(kInventoryColumns - 1, col + 1);
    }
    if (input.inventoryUpPressed) {
        row = std::max(0, row - 1);
    }
    if (input.inventoryDownPressed) {
        row = std::min(kInventoryRows - 1, row + 1);
    }

    inventoryCursor_ = row * kInventoryColumns + col;
    if (isHotbarInventoryIndex(inventoryCursor_)) {
        selectedSlot_ = inventoryCursor_ - kInventoryHotbarOffset;
        inventory_.selectedHotbarSlot = static_cast<std::uint8_t>(selectedSlot_);
    }

    if (input.inventorySelectPressed || input.clickPrimaryPressed) {
        resolveInventoryPrimaryAction();
    }
    if (input.clickSecondaryPressed) {
        resolveInventorySecondaryAction();
    }
}

void GameSession::updateInteraction(float deltaTime, const InputSnapshot& input) {
    if (!input.mouseCaptured) {
        resetBreakState();
        return;
    }

    if (!currentHit_.has_value()) {
        resetBreakState();
    } else if (input.primaryHeld) {
        if (!breakingBlock_.has_value() || *breakingBlock_ != currentHit_->block) {
            breakingBlock_ = currentHit_->block;
            breakProgress_ = 0.0f;
        }

        const float duration = breakDurationSeconds(currentHit_->blockId);
        if (duration <= 0.0f) {
            resetBreakState();
        } else {
            breakProgress_ = clampf(breakProgress_ + deltaTime / duration, 0.0f, 1.0f);
            if (breakProgress_ >= 0.999f) {
                const BlockId broken = currentHit_->blockId;
                const IVec3 block = currentHit_->block;
                if (world_.setBlock(block.x, block.y, block.z, BlockId::Air)) {
                    const Vec3 spawnPosition {static_cast<float>(block.x) + 0.5f, static_cast<float>(block.y) + 0.35f,
                                              static_cast<float>(block.z) + 0.5f};
                    Vec3 impulse = normalize(cameraPosition() - spawnPosition);
                    if (length(impulse) <= 0.0001f) {
                        impulse = {0.0f, 0.0f, 0.0f};
                    }
                    impulse = impulse * 1.1f;
                    impulse.y = std::max(impulse.y + 0.7f, 0.9f);
                    spawnDrop(droppedBlockForBlock(broken), spawnPosition, impulse);
                }
                resetBreakState();
            }
        }
    } else {
        resetBreakState();
    }

    if (input.clickSecondaryPressed && currentHit_.has_value()) {
        const IVec3 place = currentHit_->previousEmpty;
        if (!wouldClipPlayer(place)) {
            (void)placeSelectedBlock(place);
        }
    }
}

void GameSession::updateDrops(float deltaTime) {
    for (DroppedItem& item : droppedItems_) {
        item.ageSeconds += deltaTime;
        item.pickupDelaySeconds = std::max(0.0f, item.pickupDelaySeconds - deltaTime);
        moveDrop(item, deltaTime);
    }

    droppedItems_.erase(std::remove_if(droppedItems_.begin(), droppedItems_.end(),
                                       [](const DroppedItem& item) {
                                           return item.stack.empty() || item.ageSeconds >= kDropDespawnTime;
                                       }),
                        droppedItems_.end());
}

void GameSession::moveDrop(DroppedItem& item, float deltaTime) {
    item.velocity.y -= kDropGravity * deltaTime;
    item.velocity.x *= 0.985f;
    item.velocity.z *= 0.985f;

    const Vec3 motion = item.velocity * deltaTime;
    moveDropAxis(item, motion.x, 0);
    moveDropAxis(item, motion.y, 1);
    moveDropAxis(item, motion.z, 2);

    if (dropCollidesAt(item.position + Vec3 {0.0f, -0.04f, 0.0f})) {
        item.velocity.x *= 0.82f;
        item.velocity.z *= 0.82f;
    }
}

void GameSession::moveDropAxis(DroppedItem& item, float amount, int axis) {
    if (std::abs(amount) <= 0.00001f) {
        return;
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(amount) / 0.08f)));
    const float stepAmount = amount / static_cast<float>(steps);

    for (int step = 0; step < steps; ++step) {
        float* component = nullptr;
        if (axis == 0) {
            component = &item.position.x;
        } else if (axis == 1) {
            component = &item.position.y;
        } else {
            component = &item.position.z;
        }

        *component += stepAmount;
        if (!dropCollidesAt(item.position)) {
            continue;
        }

        *component -= stepAmount;
        if (axis == 0) {
            item.velocity.x = 0.0f;
        } else if (axis == 1) {
            item.velocity.y = 0.0f;
        } else {
            item.velocity.z = 0.0f;
        }
        break;
    }
}

bool GameSession::dropCollidesAt(const Vec3& position) const {
    const Vec3 min {position.x - kDropHalfSize, position.y - kDropHalfSize, position.z - kDropHalfSize};
    const Vec3 max {position.x + kDropHalfSize, position.y + kDropHalfSize, position.z + kDropHalfSize};
    return world_.collidesAabb(min, max);
}

void GameSession::tryCollectDrops() {
    const Vec3 pickupCenter = player_.position + Vec3 {0.0f, 0.9f, 0.0f};
    for (DroppedItem& item : droppedItems_) {
        if (item.pickupDelaySeconds > 0.0f ||
            distanceSquared(item.position, pickupCenter) > kDropPickupRadius * kDropPickupRadius) {
            continue;
        }

        item.stack = insertIntoInventory(item.stack);
    }

    droppedItems_.erase(std::remove_if(droppedItems_.begin(), droppedItems_.end(),
                                       [](const DroppedItem& item) { return item.stack.empty(); }),
                        droppedItems_.end());
}

void GameSession::resetBreakState() {
    breakingBlock_.reset();
    breakProgress_ = 0.0f;
}

void GameSession::resolveInventoryPrimaryAction() {
    ItemStack& slot = inventory_.slots[static_cast<std::size_t>(inventoryCursor_)];
    if (carriedStack_.empty()) {
        carriedStack_ = slot;
        slot.clear();
        return;
    }

    if (slot.empty()) {
        slot = carriedStack_;
        carriedStack_.clear();
        return;
    }

    if (canStacksMerge(slot, carriedStack_) && slot.count < kMaxStackCount) {
        const int moved = std::min<int>(kMaxStackCount - slot.count, carriedStack_.count);
        slot.count = static_cast<std::uint8_t>(slot.count + moved);
        carriedStack_.count = static_cast<std::uint8_t>(carriedStack_.count - moved);
        if (carriedStack_.count == 0) {
            carriedStack_.clear();
        }
        return;
    }

    std::swap(slot, carriedStack_);
}

void GameSession::resolveInventorySecondaryAction() {
    ItemStack& slot = inventory_.slots[static_cast<std::size_t>(inventoryCursor_)];
    if (carriedStack_.empty()) {
        if (slot.empty()) {
            return;
        }

        const int taken = (slot.count + 1) / 2;
        carriedStack_ = makeStack(slot.block, taken);
        slot.count = static_cast<std::uint8_t>(slot.count - taken);
        if (slot.count == 0) {
            slot.clear();
        }
        return;
    }

    if (slot.empty()) {
        slot = makeStack(carriedStack_.block, 1);
        carriedStack_.count = static_cast<std::uint8_t>(carriedStack_.count - 1);
        if (carriedStack_.count == 0) {
            carriedStack_.clear();
        }
        return;
    }

    if (slot.block == carriedStack_.block && slot.count < kMaxStackCount) {
        ++slot.count;
        carriedStack_.count = static_cast<std::uint8_t>(carriedStack_.count - 1);
        if (carriedStack_.count == 0) {
            carriedStack_.clear();
        }
    }
}

void GameSession::returnCarriedStackToInventory() {
    if (carriedStack_.empty()) {
        return;
    }

    carriedStack_ = insertIntoInventory(carriedStack_);
    if (!carriedStack_.empty()) {
        const Vec3 forward = viewDirection();
        spawnDrop(carriedStack_.block, cameraPosition() + forward * 0.9f,
                  forward * 1.8f + Vec3 {0.0f, 1.4f, 0.0f}, carriedStack_.count);
        carriedStack_.clear();
    }
}

void GameSession::spawnDrop(BlockId block, const Vec3& position, const Vec3& impulse, std::uint8_t count) {
    if (block == BlockId::Air || count == 0) {
        return;
    }

    DroppedItem item;
    item.stack = makeStack(block, count);
    item.position = position;
    item.velocity = impulse;
    item.pickupDelaySeconds = 0.20f;
    droppedItems_.push_back(item);
}

ItemStack GameSession::insertIntoInventory(ItemStack stack) {
    if (stack.empty()) {
        return {};
    }

    const auto mergeRange = [&](int begin, int end) {
        for (int index = begin; index < end && !stack.empty(); ++index) {
            ItemStack& slot = inventory_.slots[static_cast<std::size_t>(index)];
            if (!canStacksMerge(slot, stack) || slot.count >= kMaxStackCount) {
                continue;
            }

            const int moved = std::min<int>(kMaxStackCount - slot.count, stack.count);
            slot.count = static_cast<std::uint8_t>(slot.count + moved);
            stack.count = static_cast<std::uint8_t>(stack.count - moved);
            if (stack.count == 0) {
                stack.clear();
            }
        }
    };

    const auto fillRange = [&](int begin, int end) {
        for (int index = begin; index < end && !stack.empty(); ++index) {
            ItemStack& slot = inventory_.slots[static_cast<std::size_t>(index)];
            if (!slot.empty()) {
                continue;
            }

            slot = makeStack(stack.block, std::min<int>(stack.count, kMaxStackCount));
            stack.count = static_cast<std::uint8_t>(stack.count - slot.count);
            if (stack.count == 0) {
                stack.clear();
            }
        }
    };

    mergeRange(kInventoryHotbarOffset, kInventorySlotCount);
    mergeRange(0, kInventoryHotbarOffset);
    fillRange(kInventoryHotbarOffset, kInventorySlotCount);
    fillRange(0, kInventoryHotbarOffset);
    return stack;
}

bool GameSession::placeSelectedBlock(const IVec3& position) {
    ItemStack& held = selectedHotbarStack();
    if (held.empty()) {
        return false;
    }

    if (!world_.setBlock(position.x, position.y, position.z, held.block)) {
        return false;
    }

    held.count = static_cast<std::uint8_t>(held.count - 1);
    if (held.count == 0) {
        held.clear();
    }
    return true;
}

ItemStack& GameSession::selectedHotbarStack() {
    return inventory_.slots[static_cast<std::size_t>(hotbarInventoryIndex(selectedSlot_))];
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
