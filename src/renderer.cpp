#include "renderer.hpp"

#include <GL/gl.h>

#include <cmath>
#include <string>

namespace voxel {
namespace {

float radians(float degrees) {
    return degrees * (kPi / 180.0f);
}

void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    const float top = nearPlane * std::tan(radians(fovDegrees) * 0.5f);
    const float right = top * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void drawRect(float x, float y, float width, float height, const Color3& color) {
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void drawRectAlpha(float x, float y, float width, float height, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void drawOutline(float x, float y, float width, float height, const Color3& color, float lineWidth) {
    glColor3f(color.r, color.g, color.b);
    glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void renderMiniCube(const Color3& color, float size) {
    const float s = size * 0.5f;
    glBegin(GL_QUADS);

    glColor3f(color.r * 0.82f, color.g * 0.82f, color.b * 0.82f);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, -s, s);
    glVertex3f(-s, s, s);
    glVertex3f(-s, s, -s);

    glColor3f(color.r * 0.90f, color.g * 0.90f, color.b * 0.90f);
    glVertex3f(s, -s, s);
    glVertex3f(s, -s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, s, s);

    glColor3f(color.r, color.g, color.b);
    glVertex3f(-s, s, s);
    glVertex3f(s, s, s);
    glVertex3f(s, s, -s);
    glVertex3f(-s, s, -s);

    glColor3f(color.r * 0.58f, color.g * 0.58f, color.b * 0.58f);
    glVertex3f(-s, -s, -s);
    glVertex3f(s, -s, -s);
    glVertex3f(s, -s, s);
    glVertex3f(-s, -s, s);

    glColor3f(color.r * 0.70f, color.g * 0.70f, color.b * 0.70f);
    glVertex3f(s, -s, -s);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, s, -s);
    glVertex3f(s, s, -s);

    glColor3f(color.r * 0.94f, color.g * 0.94f, color.b * 0.94f);
    glVertex3f(-s, -s, s);
    glVertex3f(s, -s, s);
    glVertex3f(s, s, s);
    glVertex3f(-s, s, s);

    glEnd();
}

void renderSelectionBox(const RaycastHit& hit, float progress) {
    const float x0 = static_cast<float>(hit.block.x) - 0.001f;
    const float y0 = static_cast<float>(hit.block.y) - 0.001f;
    const float z0 = static_cast<float>(hit.block.z) - 0.001f;
    const float x1 = x0 + 1.002f;
    const float y1 = y0 + 1.002f;
    const float z1 = z0 + 1.002f;
    const float accent = clampf(progress, 0.0f, 1.0f);

    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f + accent * 2.0f);
    glColor3f(1.0f, 1.0f - accent * 0.20f, 1.0f - accent * 0.65f);
    glBegin(GL_LINES);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y0, z0);

    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y0, z1);

    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z1);
    glEnd();
    glEnable(GL_CULL_FACE);
}

void renderBlockIcon(BlockId block, float x, float y, float size) {
    drawRect(x, y, size, size, blockColor(block));
    drawOutline(x, y, size, size, {0.08f, 0.08f, 0.08f}, 1.0f);
}

void drawDigitSegment(float x, float y, float width, float height, const Color3& color) {
    drawRect(x, y, width, height, color);
}

void drawDigit(int digit, float x, float y, float scale, const Color3& color) {
    static constexpr int kSegments[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f,
    };
    if (digit < 0 || digit > 9) {
        return;
    }

    const float t = scale;
    const float w = scale * 2.4f;
    const float h = scale * 4.2f;
    const int mask = kSegments[digit];

    if (mask & 0x01) {
        drawDigitSegment(x + t, y, w, t, color);
    }
    if (mask & 0x02) {
        drawDigitSegment(x + t + w, y + t, t, h, color);
    }
    if (mask & 0x04) {
        drawDigitSegment(x + t + w, y + 2.0f * t + h, t, h, color);
    }
    if (mask & 0x08) {
        drawDigitSegment(x + t, y + 2.0f * (t + h), w, t, color);
    }
    if (mask & 0x10) {
        drawDigitSegment(x, y + 2.0f * t + h, t, h, color);
    }
    if (mask & 0x20) {
        drawDigitSegment(x, y + t, t, h, color);
    }
    if (mask & 0x40) {
        drawDigitSegment(x + t, y + t + h, w, t, color);
    }
}

void drawNumber(int value, float x, float y, float scale, const Color3& color) {
    const std::string text = std::to_string(value);
    for (std::size_t index = 0; index < text.size(); ++index) {
        drawDigit(text[index] - '0', x + static_cast<float>(index) * (scale * 4.4f), y, scale, color);
    }
}

void renderStackCount(const ItemStack& stack, float x, float y, float size) {
    if (stack.empty() || stack.count <= 1) {
        return;
    }

    const float scale = std::max(1.4f, size * 0.055f);
    const float digitWidth = scale * 4.4f * (stack.count >= 10 ? 2.0f : 1.0f);
    const float bgWidth = digitWidth + scale * 1.4f;
    const float bgHeight = scale * 10.2f;
    const float bgX = x + size - bgWidth - 1.0f;
    const float bgY = y + size - bgHeight - 1.0f;

    drawRectAlpha(bgX, bgY, bgWidth, bgHeight, 0.03f, 0.03f, 0.04f, 0.82f);
    drawNumber(stack.count, bgX + scale * 0.5f, bgY + scale * 0.6f, scale, {0.96f, 0.96f, 0.96f});
}

void renderDroppedItem(const DroppedItem& item) {
    if (item.stack.empty()) {
        return;
    }

    glPushMatrix();
    glTranslatef(item.position.x, item.position.y + 0.12f + std::sin(item.ageSeconds * 4.0f) * 0.04f, item.position.z);
    glRotatef(item.ageSeconds * 110.0f, 0.0f, 1.0f, 0.0f);
    renderMiniCube(blockColor(item.stack.block), 0.28f);
    glPopMatrix();
}

void renderInventorySlot(const ItemStack& stack, float x, float y, float size, bool cursor, bool selectedHotbar) {
    drawRect(x, y, size, size, {0.10f, 0.11f, 0.14f});
    const Color3 outline = cursor ? Color3 {0.95f, 0.90f, 0.32f}
                                  : (selectedHotbar ? Color3 {0.84f, 0.88f, 0.96f}
                                                    : Color3 {0.42f, 0.45f, 0.53f});
    drawOutline(x, y, size, size, outline, cursor ? 3.0f : (selectedHotbar ? 2.2f : 1.2f));
    if (!stack.empty()) {
        renderBlockIcon(stack.block, x + 6.0f, y + 6.0f, size - 12.0f);
        renderStackCount(stack, x, y, size);
    }
}

}  // namespace

bool Renderer::initialize(std::string& error) {
    error.clear();
    glDisable(GL_DITHER);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glShadeModel(GL_FLAT);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glClearColor(0.55f, 0.76f, 0.96f, 1.0f);
    return true;
}

void Renderer::render(const GameSession& session, int width, int height) const {
    if (width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    setPerspective(70.0f, static_cast<float>(width) / static_cast<float>(height), 0.05f, 180.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const PlayerState& player = session.player();
    const Vec3 camera = session.cameraPosition();
    glRotatef(-player.pitchDegrees, 1.0f, 0.0f, 0.0f);
    glRotatef(-player.yawDegrees, 0.0f, 1.0f, 0.0f);
    glTranslatef(-camera.x, -camera.y, -camera.z);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    for (const Chunk* chunk : session.world().collectRenderable(session.playerChunk())) {
        glVertexPointer(3, GL_FLOAT, 0, chunk->mesh.vertices.data());
        glColorPointer(3, GL_FLOAT, 0, chunk->mesh.colors.data());
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(chunk->mesh.vertexCount()));
    }
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    for (const DroppedItem& item : session.droppedItems()) {
        renderDroppedItem(item);
    }

    if (session.currentHit().has_value()) {
        const bool miningThisBlock =
            session.breakingBlock().has_value() && *session.breakingBlock() == session.currentHit()->block;
        renderSelectionBox(*session.currentHit(), miningThisBlock ? session.breakProgress() : 0.0f);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.5f;
    if (!session.inventoryOpen()) {
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(centerX - 6.0f, centerY);
        glVertex2f(centerX + 6.0f, centerY);
        glVertex2f(centerX, centerY - 6.0f);
        glVertex2f(centerX, centerY + 6.0f);
        glEnd();
    }

    constexpr float hotbarSlotSize = 38.0f;
    constexpr float hotbarGap = 7.0f;
    const float hotbarWidth = kHotbarSlotCount * hotbarSlotSize + (kHotbarSlotCount - 1) * hotbarGap;
    const float hotbarX = (static_cast<float>(width) - hotbarWidth) * 0.5f;
    const float hotbarY = static_cast<float>(height) - 62.0f;

    for (int index = 0; index < kHotbarSlotCount; ++index) {
        const float x = hotbarX + static_cast<float>(index) * (hotbarSlotSize + hotbarGap);
        renderInventorySlot(session.hotbarSlot(index), x, hotbarY, hotbarSlotSize, false, index == session.selectedSlot());
    }

    if (player.flying && !session.inventoryOpen()) {
        drawOutline(hotbarX - 18.0f, hotbarY + 9.0f, 10.0f, 20.0f, {0.85f, 0.95f, 1.0f}, 2.0f);
    }

    if (session.breakProgress() > 0.0f && session.breakingBlock().has_value()) {
        const float barWidth = 62.0f;
        drawRect(centerX - barWidth * 0.5f, centerY + 17.0f, barWidth, 7.0f, {0.10f, 0.10f, 0.12f});
        drawRect(centerX - barWidth * 0.5f + 1.0f, centerY + 18.0f, (barWidth - 2.0f) * session.breakProgress(), 5.0f,
                 {0.96f, 0.86f, 0.34f});
        drawOutline(centerX - barWidth * 0.5f, centerY + 17.0f, barWidth, 7.0f, {0.96f, 0.96f, 0.96f}, 1.0f);
    }

    if (!session.inventoryOpen()) {
        return;
    }

    constexpr float slotSize = 46.0f;
    constexpr float gap = 6.0f;
    constexpr float mainRows = 3.0f;
    const float gridWidth = kInventoryColumns * slotSize + (kInventoryColumns - 1) * gap;
    const float mainGridHeight = mainRows * slotSize + (mainRows - 1.0f) * gap;
    const float hotbarGridYGap = 22.0f;
    const float panelWidth = gridWidth + 36.0f;
    const float panelHeight = 44.0f + mainGridHeight + hotbarGridYGap + slotSize + 28.0f;
    const float panelX = (static_cast<float>(width) - panelWidth) * 0.5f;
    const float panelY = (static_cast<float>(height) - panelHeight) * 0.5f;
    const float gridX = panelX + 18.0f;
    const float gridY = panelY + 24.0f;
    const float hotbarGridY = gridY + mainGridHeight + hotbarGridYGap;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRectAlpha(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 0.0f, 0.58f);
    drawRectAlpha(panelX, panelY, panelWidth, panelHeight, 0.08f, 0.09f, 0.11f, 0.97f);
    glDisable(GL_BLEND);
    drawOutline(panelX, panelY, panelWidth, panelHeight, {0.90f, 0.90f, 0.96f}, 2.0f);

    const auto& slots = session.inventorySlots();
    for (int index = 0; index < kStorageSlotCount; ++index) {
        const int row = index / kInventoryColumns;
        const int col = index % kInventoryColumns;
        const float x = gridX + static_cast<float>(col) * (slotSize + gap);
        const float y = gridY + static_cast<float>(row) * (slotSize + gap);
        renderInventorySlot(slots[static_cast<std::size_t>(index)], x, y, slotSize, index == session.inventoryCursor(),
                            false);
    }

    for (int hotbarIndex = 0; hotbarIndex < kHotbarSlotCount; ++hotbarIndex) {
        const int inventoryIndex = kInventoryHotbarOffset + hotbarIndex;
        const float x = gridX + static_cast<float>(hotbarIndex) * (slotSize + gap);
        renderInventorySlot(slots[static_cast<std::size_t>(inventoryIndex)], x, hotbarGridY, slotSize,
                            inventoryIndex == session.inventoryCursor(), hotbarIndex == session.selectedSlot());
    }

    const float carrySize = 42.0f;
    const float carryX = panelX + panelWidth - carrySize - 18.0f;
    const float carryY = panelY - carrySize - 10.0f;
    renderInventorySlot(session.carriedStack(), carryX, carryY, carrySize, false, !session.carriedStack().empty());
}

}  // namespace voxel
