#include "renderer.hpp"

#include <GL/gl.h>

#include <cmath>

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

int inventoryRows(int count, int columns) {
    return (count + columns - 1) / columns;
}

void renderSelectionBox(const RaycastHit& hit) {
    const float x0 = static_cast<float>(hit.block.x) - 0.001f;
    const float y0 = static_cast<float>(hit.block.y) - 0.001f;
    const float z0 = static_cast<float>(hit.block.z) - 0.001f;
    const float x1 = x0 + 1.002f;
    const float y1 = y0 + 1.002f;
    const float z1 = z0 + 1.002f;

    glDisable(GL_CULL_FACE);
    glLineWidth(2.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
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

    if (session.currentHit().has_value()) {
        renderSelectionBox(*session.currentHit());
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

    constexpr float slotSize = 34.0f;
    constexpr float gap = 8.0f;
    const float totalWidth = session.hotbar().size() * slotSize + (session.hotbar().size() - 1) * gap;
    const float startX = (static_cast<float>(width) - totalWidth) * 0.5f;
    const float startY = static_cast<float>(height) - 54.0f;

    for (std::size_t index = 0; index < session.hotbar().size(); ++index) {
        const float x = startX + static_cast<float>(index) * (slotSize + gap);
        drawRect(x, startY, slotSize, slotSize, {0.08f, 0.08f, 0.09f});
        drawOutline(x, startY, slotSize, slotSize,
                    index == static_cast<std::size_t>(session.selectedSlot()) ? Color3 {0.95f, 0.90f, 0.32f}
                                                                              : Color3 {0.75f, 0.75f, 0.78f},
                    index == static_cast<std::size_t>(session.selectedSlot()) ? 3.0f : 1.5f);
        renderBlockIcon(session.hotbar()[index], x + 7.0f, startY + 7.0f, slotSize - 14.0f);
    }

    if (player.flying && !session.inventoryOpen()) {
        drawOutline(startX - 18.0f, startY + 8.0f, 10.0f, 18.0f, {0.85f, 0.95f, 1.0f}, 2.0f);
    }

    if (!session.inventoryOpen()) {
        return;
    }

    constexpr int kColumns = 4;
    constexpr float inventorySlotSize = 58.0f;
    constexpr float inventoryGap = 12.0f;
    const int count = static_cast<int>(session.creativeInventory().size());
    const int rows = inventoryRows(count, kColumns);
    const float panelWidth = kColumns * inventorySlotSize + (kColumns - 1) * inventoryGap + 44.0f;
    const float panelHeight = rows * inventorySlotSize + (rows - 1) * inventoryGap + 54.0f;
    const float panelX = (static_cast<float>(width) - panelWidth) * 0.5f;
    const float panelY = (static_cast<float>(height) - panelHeight) * 0.5f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRectAlpha(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 0.0f, 0.50f);
    drawRectAlpha(panelX, panelY, panelWidth, panelHeight, 0.08f, 0.09f, 0.12f, 0.96f);
    glDisable(GL_BLEND);
    drawOutline(panelX, panelY, panelWidth, panelHeight, {0.90f, 0.90f, 0.96f}, 2.0f);

    for (int index = 0; index < count; ++index) {
        const int row = index / kColumns;
        const int col = index % kColumns;
        const float x = panelX + 22.0f + col * (inventorySlotSize + inventoryGap);
        const float y = panelY + 28.0f + row * (inventorySlotSize + inventoryGap);
        drawRect(x, y, inventorySlotSize, inventorySlotSize, {0.11f, 0.12f, 0.15f});
        drawOutline(x, y, inventorySlotSize, inventorySlotSize,
                    index == session.inventoryCursor() ? Color3 {0.95f, 0.90f, 0.32f}
                                                       : Color3 {0.54f, 0.57f, 0.64f},
                    index == session.inventoryCursor() ? 3.0f : 1.5f);
        renderBlockIcon(session.creativeInventory()[index], x + 11.0f, y + 11.0f, inventorySlotSize - 22.0f);
    }
}

}  // namespace voxel
