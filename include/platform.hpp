#pragma once

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>

#include <array>
#include <chrono>
#include <string>

#include "input.hpp"

namespace voxel {

struct PlatformConfig {
    int width = 960;
    int height = 600;
    std::string title = "VoxelCraft";
};

class Platform {
public:
    Platform() = default;
    ~Platform();

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    bool create(const PlatformConfig& config, std::string& error);
    PlatformFrame pollFrame();
    void swapBuffers();
    void setTitle(const std::string& title);
    [[nodiscard]] bool isOpen() const {
        return open_;
    }
    [[nodiscard]] int width() const {
        return width_;
    }
    [[nodiscard]] int height() const {
        return height_;
    }

private:
    void destroy();
    void refreshFocusState();
    void captureMouse();
    void releaseMouse();
    void centerCursor();
    void updateMouseDelta(InputSnapshot& input);
    void resetKeyboardState();
    void syncKeyboardState();
    void applyDiscreteInput(InputSnapshot& input);
    void handleKeyPress(KeyCode keycode);
    void updateContinuousInput(InputSnapshot& input) const;
    [[nodiscard]] bool isPressed(KeyCode keycode) const;
    [[nodiscard]] bool wasPressedThisFrame(KeyCode keycode) const;
    [[nodiscard]] bool isAutoRepeatRelease(const XEvent& event) const;
    [[nodiscard]] double nowSeconds() const;

    Display* display_ = nullptr;
    Window window_ = 0;
    GLXContext context_ = nullptr;
    Colormap colorMap_ = 0;
    Cursor hiddenCursor_ = 0;
    Atom wmDeleteMessage_ = 0;
    bool open_ = false;
    bool focused_ = false;
    bool mouseCaptured_ = false;
    int width_ = 0;
    int height_ = 0;
    std::chrono::steady_clock::time_point startTime_ {};
    double lastFrameTime_ = 0.0;
    std::array<bool, 256> pressedKeys_ {};
    std::array<bool, 256> pressedThisFrame_ {};
    std::array<KeyCode, 9> slotKeys_ {};
    KeyCode keyW_ = 0;
    KeyCode keyA_ = 0;
    KeyCode keyS_ = 0;
    KeyCode keyD_ = 0;
    KeyCode keySpace_ = 0;
    KeyCode keyShiftLeft_ = 0;
    KeyCode keyShiftRight_ = 0;
    KeyCode keyCtrlLeft_ = 0;
    KeyCode keyCtrlRight_ = 0;
    KeyCode keyF_ = 0;
    KeyCode keyE_ = 0;
    KeyCode keyEscape_ = 0;
    KeyCode keyUp_ = 0;
    KeyCode keyDown_ = 0;
    KeyCode keyLeft_ = 0;
    KeyCode keyRight_ = 0;
    KeyCode keyEnter_ = 0;
};

}  // namespace voxel
