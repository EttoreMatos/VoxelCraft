#include "texture.hpp"

#include <GL/gl.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace voxel {
namespace {

std::string shellQuote(const std::string& text) {
    std::string quoted = "'";
    for (const char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

constexpr int kAtlasSize = 64;
constexpr int kAtlasBytes = kAtlasSize * kAtlasSize * 4;

}  // namespace

TextureAtlas::~TextureAtlas() {
    shutdown();
}

bool TextureAtlas::initialize(const std::string& minecraftSource, std::string& warningOrError) {
    shutdown();
    warningOrError.clear();

    if (!minecraftSource.empty()) {
        if (!prepareFromSource(minecraftSource, warningOrError)) {
            return false;
        }
    }

    const auto atlasPath = std::filesystem::path("assets/generated/minecraft_atlas.rgba");
    if (!std::filesystem::exists(atlasPath)) {
        return true;
    }

    return loadGeneratedAtlas(warningOrError);
}

void TextureAtlas::shutdown() {
    if (textureId_ != 0U) {
        glDeleteTextures(1, &textureId_);
        textureId_ = 0U;
    }
    available_ = false;
}

bool TextureAtlas::prepareFromSource(const std::filesystem::path& source, std::string& warningOrError) {
    std::filesystem::create_directories("assets/generated");
    const std::string command =
        "python3 tools/prepare_minecraft_textures.py --source " + shellQuote(source.string()) +
        " --out " + shellQuote("assets/generated/minecraft_atlas.rgba");

    const int result = std::system(command.c_str());
    if (result != 0) {
        std::ostringstream message;
        message << "Falha ao preparar as texturas oficiais do Minecraft a partir de '" << source.string() << "'.";
        warningOrError = message.str();
        return false;
    }

    return true;
}

bool TextureAtlas::loadGeneratedAtlas(std::string& warningOrError) {
    std::ifstream input("assets/generated/minecraft_atlas.rgba", std::ios::binary);
    if (!input) {
        warningOrError = "Nao consegui abrir o atlas de texturas gerado.";
        return false;
    }

    const std::vector<std::uint8_t> pixels((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (static_cast<int>(pixels.size()) != kAtlasBytes) {
        warningOrError = "O atlas gerado tem tamanho invalido.";
        return false;
    }

    return uploadRgba(pixels, kAtlasSize, kAtlasSize, warningOrError);
}

bool TextureAtlas::uploadRgba(const std::vector<std::uint8_t>& pixels, int width, int height,
                              std::string& warningOrError) {
    if (textureId_ == 0U) {
        glGenTextures(1, &textureId_);
    }
    if (textureId_ == 0U) {
        warningOrError = "Nao consegui criar a textura OpenGL.";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, textureId_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#ifdef GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    available_ = true;
    return true;
}

}  // namespace voxel
