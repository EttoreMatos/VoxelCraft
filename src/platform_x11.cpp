#include "platform.hpp"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

namespace voxel {
namespace {

bool isValidKeycode(KeyCode keycode) {
    return keycode != 0;
}

bool queryKeyState(const std::array<char, 32>& keymap, KeyCode keycode) {
    if (!isValidKeycode(keycode)) {
        return false;
    }

    const std::size_t byteIndex = static_cast<std::size_t>(keycode) / 8;
    const unsigned int bitMask = 1u << (static_cast<unsigned int>(keycode) & 7u);
    return (static_cast<unsigned char>(keymap[byteIndex]) & bitMask) != 0;
}

}  // namespace

Platform::~Platform() {
    destroy();
}

bool Platform::create(const PlatformConfig& config, std::string& error) {
    destroy();

    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        error = "Nao consegui abrir o display X11.";
        return false;
    }

    const int screen = DefaultScreen(display_);
    int visualAttributes[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        None,
    };

    XVisualInfo* visualInfo = glXChooseVisual(display_, screen, visualAttributes);
    if (visualInfo == nullptr) {
        error = "Nao encontrei um visual GLX compativel.";
        destroy();
        return false;
    }

    colorMap_ = XCreateColormap(display_, RootWindow(display_, screen), visualInfo->visual, AllocNone);
    XSetWindowAttributes attributes {};
    attributes.colormap = colorMap_;
    attributes.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                            StructureNotifyMask | FocusChangeMask | PointerMotionMask;

    width_ = config.width;
    height_ = config.height;
    window_ = XCreateWindow(display_, RootWindow(display_, screen), 0, 0, static_cast<unsigned int>(width_),
                            static_cast<unsigned int>(height_), 0, visualInfo->depth, InputOutput,
                            visualInfo->visual, CWColormap | CWEventMask, &attributes);
    if (window_ == 0) {
        XFree(visualInfo);
        error = "Falha ao criar a janela X11.";
        destroy();
        return false;
    }

    XStoreName(display_, window_, config.title.c_str());
    XWMHints wmHints {};
    wmHints.flags = InputHint;
    wmHints.input = True;
    XSetWMHints(display_, window_, &wmHints);
    wmDeleteMessage_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display_, window_, &wmDeleteMessage_, 1);

    context_ = glXCreateContext(display_, visualInfo, nullptr, True);
    XFree(visualInfo);
    if (context_ == nullptr) {
        error = "Falha ao criar o contexto OpenGL.";
        destroy();
        return false;
    }

    XMapRaised(display_, window_);
    glXMakeCurrent(display_, window_, context_);

    char cursorData[1] = {0};
    Pixmap blank = XCreateBitmapFromData(display_, window_, cursorData, 1, 1);
    XColor black {};
    hiddenCursor_ = XCreatePixmapCursor(display_, blank, blank, &black, &black, 0, 0);
    XFreePixmap(display_, blank);
    XSync(display_, False);
    refreshFocusState();

    keyW_ = XKeysymToKeycode(display_, XK_w);
    keyA_ = XKeysymToKeycode(display_, XK_a);
    keyS_ = XKeysymToKeycode(display_, XK_s);
    keyD_ = XKeysymToKeycode(display_, XK_d);
    keySpace_ = XKeysymToKeycode(display_, XK_space);
    keyShiftLeft_ = XKeysymToKeycode(display_, XK_Shift_L);
    keyShiftRight_ = XKeysymToKeycode(display_, XK_Shift_R);
    keyCtrlLeft_ = XKeysymToKeycode(display_, XK_Control_L);
    keyCtrlRight_ = XKeysymToKeycode(display_, XK_Control_R);
    keyF_ = XKeysymToKeycode(display_, XK_f);
    keyE_ = XKeysymToKeycode(display_, XK_e);
    keyEscape_ = XKeysymToKeycode(display_, XK_Escape);
    keyUp_ = XKeysymToKeycode(display_, XK_Up);
    keyDown_ = XKeysymToKeycode(display_, XK_Down);
    keyLeft_ = XKeysymToKeycode(display_, XK_Left);
    keyRight_ = XKeysymToKeycode(display_, XK_Right);
    keyEnter_ = XKeysymToKeycode(display_, XK_Return);
    slotKeys_[0] = XKeysymToKeycode(display_, XK_1);
    slotKeys_[1] = XKeysymToKeycode(display_, XK_2);
    slotKeys_[2] = XKeysymToKeycode(display_, XK_3);
    slotKeys_[3] = XKeysymToKeycode(display_, XK_4);
    slotKeys_[4] = XKeysymToKeycode(display_, XK_5);
    slotKeys_[5] = XKeysymToKeycode(display_, XK_6);
    slotKeys_[6] = XKeysymToKeycode(display_, XK_7);
    slotKeys_[7] = XKeysymToKeycode(display_, XK_8);
    slotKeys_[8] = XKeysymToKeycode(display_, XK_9);

    startTime_ = std::chrono::steady_clock::now();
    lastFrameTime_ = 0.0;
    open_ = true;
    mouseCaptured_ = false;
    resetKeyboardState();
    refreshFocusState();
    return true;
}

void Platform::destroy() {
    releaseMouse();
    if (display_ != nullptr && context_ != nullptr) {
        glXMakeCurrent(display_, None, nullptr);
        glXDestroyContext(display_, context_);
        context_ = nullptr;
    }
    if (display_ != nullptr && hiddenCursor_ != 0) {
        XFreeCursor(display_, hiddenCursor_);
        hiddenCursor_ = 0;
    }
    if (display_ != nullptr && window_ != 0) {
        XDestroyWindow(display_, window_);
        window_ = 0;
    }
    if (display_ != nullptr && colorMap_ != 0) {
        XFreeColormap(display_, colorMap_);
        colorMap_ = 0;
    }
    if (display_ != nullptr) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }

    open_ = false;
    focused_ = false;
}

void Platform::refreshFocusState() {
    if (display_ == nullptr || window_ == 0) {
        focused_ = false;
        return;
    }

    Window focusedWindow = 0;
    int revertTo = RevertToNone;
    XGetInputFocus(display_, &focusedWindow, &revertTo);
    focused_ = focusedWindow == window_;
}

PlatformFrame Platform::pollFrame() {
    PlatformFrame frame;
    frame.width = width_;
    frame.height = height_;

    if (!open_ || display_ == nullptr) {
        frame.input.quitRequested = true;
        return frame;
    }

    const double now = nowSeconds();
    frame.deltaTime = lastFrameTime_ <= 0.0 ? 0.0 : now - lastFrameTime_;
    lastFrameTime_ = now;
    pressedThisFrame_.fill(false);

    while (XPending(display_) > 0) {
        XEvent event {};
        XNextEvent(display_, &event);

        switch (event.type) {
            case ClientMessage:
                if (static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteMessage_) {
                    open_ = false;
                    frame.input.quitRequested = true;
                }
                break;
            case ConfigureNotify:
                width_ = event.xconfigure.width;
                height_ = event.xconfigure.height;
                frame.width = width_;
                frame.height = height_;
                frame.input.resized = true;
                break;
            case FocusIn:
                focused_ = true;
                break;
            case FocusOut:
                focused_ = false;
                releaseMouse();
                resetKeyboardState();
                break;
            case KeyPress:
                handleKeyPress(event.xkey.keycode);
                break;
            case KeyRelease:
                if (!isAutoRepeatRelease(event) && isValidKeycode(event.xkey.keycode)) {
                    pressedKeys_[static_cast<std::size_t>(event.xkey.keycode)] = false;
                }
                break;
            case ButtonPress:
                if (event.xbutton.button == Button1) {
                    if (!mouseCaptured_) {
                        focused_ = true;
                    }
                    if (focused_ && !mouseCaptured_) {
                        captureMouse();
                    } else if (mouseCaptured_) {
                        frame.input.clickPrimaryPressed = true;
                    }
                } else if (event.xbutton.button == Button3 && mouseCaptured_) {
                    frame.input.clickSecondaryPressed = true;
                }
                break;
            default:
                break;
        }
    }

    refreshFocusState();

    const bool inputActive = focused_ || mouseCaptured_;

    if (inputActive) {
        syncKeyboardState();
        if (mouseCaptured_) {
            updateMouseDelta(frame.input);
        }
    } else {
        resetKeyboardState();
    }

    applyDiscreteInput(frame.input);
    updateContinuousInput(frame.input);
    frame.input.focused = inputActive;
    frame.input.mouseCaptured = mouseCaptured_;

    if (!open_) {
        frame.input.quitRequested = true;
    }
    return frame;
}

void Platform::swapBuffers() {
    if (display_ != nullptr && window_ != 0) {
        glXSwapBuffers(display_, window_);
    }
}

void Platform::setTitle(const std::string& title) {
    if (display_ != nullptr && window_ != 0) {
        XStoreName(display_, window_, title.c_str());
    }
}

void Platform::captureMouse() {
    if (display_ == nullptr || window_ == 0 || mouseCaptured_) {
        return;
    }

    const int grabResult = XGrabPointer(display_, window_, True,
                                        ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                                        GrabModeAsync, window_, hiddenCursor_, CurrentTime);
    if (grabResult != GrabSuccess) {
        return;
    }

    mouseCaptured_ = true;
    focused_ = true;
    XDefineCursor(display_, window_, hiddenCursor_);
    centerCursor();
    XFlush(display_);
}

void Platform::releaseMouse() {
    if (display_ == nullptr || window_ == 0 || !mouseCaptured_) {
        return;
    }

    XUngrabPointer(display_, CurrentTime);
    XUndefineCursor(display_, window_);
    mouseCaptured_ = false;
    XFlush(display_);
}

void Platform::centerCursor() {
    if (display_ == nullptr || window_ == 0 || width_ <= 0 || height_ <= 0) {
        return;
    }
    XWarpPointer(display_, None, window_, 0, 0, 0, 0, width_ / 2, height_ / 2);
    XFlush(display_);
}

void Platform::updateMouseDelta(InputSnapshot& input) {
    Window root = 0;
    Window child = 0;
    int rootX = 0;
    int rootY = 0;
    int winX = 0;
    int winY = 0;
    unsigned int mask = 0;

    if (!XQueryPointer(display_, window_, &root, &child, &rootX, &rootY, &winX, &winY, &mask)) {
        return;
    }

    const int centerX = width_ / 2;
    const int centerY = height_ / 2;
    input.mouseDx = static_cast<float>(winX - centerX);
    input.mouseDy = static_cast<float>(winY - centerY);
    input.primaryHeld = (mask & Button1Mask) != 0;
    input.secondaryHeld = (mask & Button3Mask) != 0;

    if (winX != centerX || winY != centerY) {
        centerCursor();
    }
}

void Platform::resetKeyboardState() {
    pressedKeys_.fill(false);
    pressedThisFrame_.fill(false);
}

void Platform::syncKeyboardState() {
    if (display_ == nullptr) {
        return;
    }

    std::array<char, 32> keymap {};
    XQueryKeymap(display_, keymap.data());

    const auto syncKey = [&](KeyCode keycode) {
        if (!isValidKeycode(keycode)) {
            return;
        }
        const bool down = queryKeyState(keymap, keycode);
        const std::size_t index = static_cast<std::size_t>(keycode);
        if (down && !pressedKeys_[index]) {
            pressedThisFrame_[index] = true;
        }
        pressedKeys_[index] = down;
    };

    syncKey(keyW_);
    syncKey(keyA_);
    syncKey(keyS_);
    syncKey(keyD_);
    syncKey(keySpace_);
    syncKey(keyShiftLeft_);
    syncKey(keyShiftRight_);
    syncKey(keyCtrlLeft_);
    syncKey(keyCtrlRight_);
    syncKey(keyF_);
    syncKey(keyE_);
    syncKey(keyEscape_);
    syncKey(keyUp_);
    syncKey(keyDown_);
    syncKey(keyLeft_);
    syncKey(keyRight_);
    syncKey(keyEnter_);
    for (const KeyCode slotKey : slotKeys_) {
        syncKey(slotKey);
    }
}

void Platform::applyDiscreteInput(InputSnapshot& input) {
    if (wasPressedThisFrame(keyEscape_)) {
        releaseMouse();
        open_ = false;
        input.quitRequested = true;
        return;
    }

    input.toggleFlyPressed = wasPressedThisFrame(keyF_);
    input.toggleInventoryPressed = wasPressedThisFrame(keyE_);
    input.inventoryUpPressed = wasPressedThisFrame(keyUp_);
    input.inventoryDownPressed = wasPressedThisFrame(keyDown_);
    input.inventoryLeftPressed = wasPressedThisFrame(keyLeft_);
    input.inventoryRightPressed = wasPressedThisFrame(keyRight_);
    input.inventorySelectPressed = wasPressedThisFrame(keyEnter_) || wasPressedThisFrame(keySpace_);
    for (std::size_t index = 0; index < slotKeys_.size(); ++index) {
        if (wasPressedThisFrame(slotKeys_[index])) {
            input.hotbarSelection = static_cast<int>(index + 1);
            break;
        }
    }
}

void Platform::handleKeyPress(KeyCode keycode) {
    if (!isValidKeycode(keycode)) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(keycode);
    if (!pressedKeys_[index]) {
        pressedThisFrame_[index] = true;
    }
    pressedKeys_[index] = true;
}

void Platform::updateContinuousInput(InputSnapshot& input) const {
    input.forward = isPressed(keyW_);
    input.back = isPressed(keyS_);
    input.left = isPressed(keyA_);
    input.right = isPressed(keyD_);
    input.jump = isPressed(keySpace_);
    input.sprint = isPressed(keyShiftLeft_) || isPressed(keyShiftRight_);
    input.descend = isPressed(keyCtrlLeft_) || isPressed(keyCtrlRight_);
}

bool Platform::isPressed(KeyCode keycode) const {
    return isValidKeycode(keycode) && pressedKeys_[static_cast<std::size_t>(keycode)];
}

bool Platform::wasPressedThisFrame(KeyCode keycode) const {
    return isValidKeycode(keycode) && pressedThisFrame_[static_cast<std::size_t>(keycode)];
}

bool Platform::isAutoRepeatRelease(const XEvent& event) const {
    if (event.type != KeyRelease || display_ == nullptr || XPending(display_) <= 0) {
        return false;
    }

    XEvent peek {};
    XPeekEvent(display_, &peek);
    return peek.type == KeyPress && peek.xkey.time == event.xkey.time &&
           peek.xkey.keycode == event.xkey.keycode;
}

double Platform::nowSeconds() const {
    const auto elapsed = std::chrono::steady_clock::now() - startTime_;
    return std::chrono::duration<double>(elapsed).count();
}

}  // namespace voxel
