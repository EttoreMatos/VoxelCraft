#pragma once

#include <array>
#include <optional>
#include <vector>

#include "input.hpp"
#include "world.hpp"

namespace voxel {

class GameSession {
public:
    explicit GameSession(WorldConfig config);

    void initialize();
    void update(float deltaTime, const InputSnapshot& input);
    void shutdown();

    [[nodiscard]] const World& world() const {
        return world_;
    }

    [[nodiscard]] World& world() {
        return world_;
    }

    [[nodiscard]] const PlayerState& player() const {
        return player_;
    }

    [[nodiscard]] const std::optional<RaycastHit>& currentHit() const {
        return currentHit_;
    }

    [[nodiscard]] const std::array<BlockId, 7>& hotbar() const {
        return hotbar_;
    }

    [[nodiscard]] const std::vector<BlockId>& creativeInventory() const {
        return creativeInventory_;
    }

    [[nodiscard]] bool inventoryOpen() const {
        return inventoryOpen_;
    }

    [[nodiscard]] int inventoryCursor() const {
        return inventoryCursor_;
    }

    [[nodiscard]] int selectedSlot() const {
        return selectedSlot_;
    }

    [[nodiscard]] ChunkCoord playerChunk() const;
    [[nodiscard]] Vec3 cameraPosition() const;
    [[nodiscard]] Vec3 viewDirection() const;

private:
    [[nodiscard]] Vec3 playerMin() const;
    [[nodiscard]] Vec3 playerMax() const;
    [[nodiscard]] bool wouldClipPlayer(const IVec3& block) const;

    void updateView(const InputSnapshot& input);
    void updateMovement(float deltaTime, const InputSnapshot& input);
    void updateInventory(const InputSnapshot& input);
    void updateInteraction(const InputSnapshot& input);
    void moveAndCollide(const Vec3& motion);
    void moveAxis(float amount, int axis);

    World world_;
    PlayerState player_;
    std::array<BlockId, 7> hotbar_ {};
    std::vector<BlockId> creativeInventory_;
    int selectedSlot_ = 0;
    int inventoryCursor_ = 0;
    bool inventoryOpen_ = false;
    std::optional<RaycastHit> currentHit_;
};

}  // namespace voxel
