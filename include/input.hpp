#pragma once

namespace voxel {

struct InputSnapshot {
    bool quitRequested = false;
    bool focused = false;
    bool mouseCaptured = false;
    bool resized = false;
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool sprint = false;
    bool descend = false;
    bool toggleFlyPressed = false;
    bool toggleInventoryPressed = false;
    bool inventoryUpPressed = false;
    bool inventoryDownPressed = false;
    bool inventoryLeftPressed = false;
    bool inventoryRightPressed = false;
    bool inventorySelectPressed = false;
    bool clickPrimaryPressed = false;
    bool clickSecondaryPressed = false;
    int hotbarSelection = 0;
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
};

struct PlatformFrame {
    InputSnapshot input;
    int width = 0;
    int height = 0;
    double deltaTime = 0.0;
};

}  // namespace voxel
