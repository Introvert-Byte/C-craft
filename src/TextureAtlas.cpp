#include "TextureAtlas.h"

#include <glad/glad.h>
#include <cstring>
#include <numeric>

namespace voxel {

namespace {
// Simple XOR-shift PRNG for procedural texture generation.
std::uint32_t rngState = 0xC0FFEEu;
inline std::uint32_t xorshift32() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

inline glm::vec4 blockBaseColor(BlockID id) {
    switch (id) {
        case BLOCK_GRASS: return glm::vec4(0.34f, 0.62f, 0.14f, 1.0f);
        case BLOCK_DIRT:  return glm::vec4(0.46f, 0.30f, 0.18f, 1.0f);
        case BLOCK_STONE: return glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
        case BLOCK_SAND:  return glm::vec4(0.82f, 0.74f, 0.55f, 1.0f);
        case BLOCK_WOOD:  return glm::vec4(0.55f, 0.32f, 0.20f, 1.0f);
        case BLOCK_LEAVES:return glm::vec4(0.20f, 0.45f, 0.20f, 1.0f);
        default:          return glm::vec4(0.20f, 0.20f, 0.25f, 1.0f);
    }
}

// Fill one tile of the atlas with a stylised face texture.
void fillTile(std::vector<std::uint8_t>& px, std::uint32_t tileSize,
              std::uint32_t tileX, std::uint32_t tileY, BlockID id, Face face) {
    const auto base = blockBaseColor(id);
    const std::uint32_t stride = tileSize * 4;

    for (std::uint32_t y = 0; y < tileSize; ++y) {
        for (std::uint32_t x = 0; x < tileSize; ++x) {
            const std::uint32_t ox = tileX * tileSize + x;
            const std::uint32_t oy = tileY * tileSize + y;
            const std::uint32_t i = oy * stride + ox * 4;

            glm::vec4 color = base;
            switch (face) {
                case Face::TOP:    color *= glm::vec4(1.05f, 1.05f, 0.9f, 1.0f); break;
                case Face::BOTTOM: color *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f); break;
                case Face::LEFT:   color *= glm::vec4(0.85f, 0.85f, 0.85f, 1.0f); break;
                case Face::RIGHT:  color *= glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);   break;
                case Face::FRONT:  color *= glm::vec4(0.95f, 0.95f, 0.95f, 1.0f); break;
                case Face::BACK:   color *= glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);   break;
                default: break;
            }

            std::uint32_t n = xorshift32() % 40;
            float grain = (n - 20) / 255.0f;
            color = glm::clamp(color + glm::vec4(grain, grain, grain, 0.0f), 0.0f, 1.0f);

            if (id == BLOCK_GRASS && face == Face::RIGHT && y > tileSize * 2 / 3) {
                color = blockBaseColor(BLOCK_DIRT);
                color *= glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
            }

            px[i + 0] = static_cast<std::uint8_t>(color.r * 255.0f);
            px[i + 1] = static_cast<std::uint8_t>(color.g * 255.0f);
            px[i + 2] = static_cast<std::uint8_t>(color.b * 255.0f);
            px[i + 3] = static_cast<std::uint8_t>(color.a * 255.0f);
        }
    }
}
} // namespace

TextureAtlas::TextureAtlas() = default;

void TextureAtlas::buildProcedural(std::uint32_t tileSize,
                                   std::uint32_t tilesPerRow) {
    m_tileSize = tileSize;
    m_tilesPerRow = tilesPerRow;

    // Assign tile indices: 6 faces per block id.
    // Note: use `int` for the loop variable since BlockID (uint8_t) would
    // overflow at 256 and create an infinite loop.
    std::uint32_t nextTile = 0;
    for (int id = 0; id < 256; ++id) {
        for (std::uint32_t f = 0; f < 6; ++f) {
            m_tileIndex[id][f] = nextTile++;
        }
    }

    std::uint32_t rows = (nextTile + tilesPerRow - 1) / tilesPerRow;
    m_atlasWidth = tilesPerRow * tileSize;
    m_atlasHeight = rows * tileSize;

    std::vector<std::uint8_t> px(m_atlasWidth * m_atlasHeight * 4, 0);

    for (int id = 0; id < 256; ++id) {
        const auto& props = blocks().get(static_cast<BlockID>(id));
        if (props.id == BLOCK_AIR && id != 0) continue;
        for (std::uint32_t f = 0; f < 6; ++f) {
            std::uint32_t tile = m_tileIndex[id][f];
            std::uint32_t tx = tile % tilesPerRow;
            std::uint32_t ty = tile / tilesPerRow;
            fillTile(px, tileSize, tx, ty, id, static_cast<Face>(f));
        }
    }

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_atlasWidth, m_atlasHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool TextureAtlas::loadFromFile(const char* /*path*/, std::uint32_t /*tileSize*/) {
    return false;
}

glm::vec2 TextureAtlas::tileUV(BlockID id, Face face) const {
    std::uint32_t tile = m_tileIndex[id][static_cast<std::uint32_t>(face)];
    std::uint32_t tx = tile % m_tilesPerRow;
    std::uint32_t ty = tile / m_tilesPerRow;
    return glm::vec2(static_cast<float>(tx) * m_tileSize / static_cast<float>(m_atlasWidth),
                     static_cast<float>(ty) * m_tileSize / static_cast<float>(m_atlasHeight));
}

glm::vec4 TextureAtlas::tileRect(BlockID id, Face face) const {
    glm::vec2 origin = tileUV(id, face);
    float tw = static_cast<float>(m_tileSize) / static_cast<float>(m_atlasWidth);
    float th = static_cast<float>(m_tileSize) / static_cast<float>(m_atlasHeight);
    return glm::vec4(origin, glm::vec2(tw, th));
}

void TextureAtlas::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
}

} // namespace voxel
