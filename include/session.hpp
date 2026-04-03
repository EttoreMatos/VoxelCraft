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

    [[nodiscard]] const std::array<ItemStack, kInventorySlotCount>& inventorySlots() const {
        return inventory_.slots;
    }

    [[nodiscard]] const ItemStack& hotbarSlot(int index) const {
        return inventory_.slots[static_cast<std::size_t>(kInventoryHotbarOffset + index)];
    }

    [[nodiscard]] const ItemStack& carriedStack() const {
        return carriedStack_;
    }

    [[nodiscard]] const std::vector<DroppedItem>& droppedItems() const {
        return droppedItems_;
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

    [[nodiscard]] float breakProgress() const {
        return breakProgress_;
    }

    [[nodiscard]] const std::optional<IVec3>& breakingBlock() const {
        return breakingBlock_;
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
    void updateInteraction(float deltaTime, const InputSnapshot& input);
    void updateDrops(float deltaTime);
    void moveDrop(DroppedItem& item, float deltaTime);
    void moveDropAxis(DroppedItem& item, float amount, int axis);
    [[nodiscard]] bool dropCollidesAt(const Vec3& position) const;
    void tryCollectDrops();
    void resetBreakState();
    void resolveInventoryPrimaryAction();
    void resolveInventorySecondaryAction();
    void returnCarriedStackToInventory();
    void spawnDrop(BlockId block, const Vec3& position, const Vec3& impulse, std::uint8_t count = 1);
    ItemStack insertIntoInventory(ItemStack stack);
    bool placeSelectedBlock(const IVec3& position);
    [[nodiscard]] ItemStack& selectedHotbarStack();
    void moveAndCollide(const Vec3& motion);
    void moveAxis(float amount, int axis);

    World world_;
    PlayerState player_;
    InventoryState inventory_;
    ItemStack carriedStack_;
    std::vector<DroppedItem> droppedItems_;
    int selectedSlot_ = 0;
    int inventoryCursor_ = 0;
    bool inventoryOpen_ = false;
    std::optional<RaycastHit> currentHit_;
    std::optional<IVec3> breakingBlock_;
    float breakProgress_ = 0.0f;
};

}  // namespace voxel
