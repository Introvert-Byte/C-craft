#include "TextureAtlas.h"

#include <glad/glad.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <cmath>

namespace voxel {

namespace {
// Deterministic noise for texture generation.
std::uint32_t rngState = 0x12345678u;
inline void rngSeed(std::uint32_t seed) { rngState = seed; }
inline std::uint32_t rngNext() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}
inline float rngFloat() { return static_cast<float>(rngNext()) / 4294967295.0f; }

inline glm::vec4 blockBaseColor(BlockID id) {
    switch (id) {
        case BLOCK_GRASS:  return glm::vec4(0.40f, 0.68f, 0.12f, 1.0f);
        case BLOCK_DIRT:   return glm::vec4(0.52f, 0.35f, 0.24f, 1.0f);
        case BLOCK_STONE:  return glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
        case BLOCK_SAND:   return glm::vec4(0.84f, 0.76f, 0.58f, 1.0f);
        case BLOCK_WOOD:   return glm::vec4(0.58f, 0.35f, 0.22f, 1.0f);
        case BLOCK_LEAVES: return glm::vec4(0.22f, 0.52f, 0.22f, 1.0f);
        case BLOCK_WATER:  return glm::vec4(0.20f, 0.45f, 0.85f, 0.6f);
        case BLOCK_COAL_ORE:   return glm::vec4(0.50f, 0.50f, 0.50f, 1.0f);
        case BLOCK_IRON_ORE:   return glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
        case BLOCK_GOLD_ORE:   return glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
        case BLOCK_DIAMOND_ORE:return glm::vec4(0.55f, 0.55f, 0.58f, 1.0f);
        default:               return glm::vec4(0.20f, 0.20f, 0.25f, 1.0f);
    }
}

inline glm::vec4 oreSpeckColor(BlockID id) {
    switch (id) {
        case BLOCK_COAL_ORE:   return glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
        case BLOCK_IRON_ORE:   return glm::vec4(0.78f, 0.78f, 0.78f, 1.0f);
        case BLOCK_GOLD_ORE:   return glm::vec4(0.88f, 0.72f, 0.18f, 1.0f);
        case BLOCK_DIAMOND_ORE:return glm::vec4(0.30f, 0.85f, 0.95f, 1.0f);
        default:               return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

// Fill one tile with a Minecraft-style block texture.
void fillTile(std::vector<std::uint8_t>& px, std::uint32_t tileSize,
              std::uint32_t tileX, std::uint32_t tileY, BlockID id, Face face) {
    const auto base = blockBaseColor(id);
    const std::uint32_t stride = tileSize * 4;

    // Face shading.
    glm::vec4 shade = glm::vec4(1.0f);
    switch (face) {
        case Face::TOP:    shade = glm::vec4(1.05f, 1.05f, 0.98f, 1.0f); break;
        case Face::BOTTOM: shade = glm::vec4(0.72f, 0.72f, 0.72f, 1.0f); break;
        case Face::LEFT:   shade = glm::vec4(0.82f, 0.82f, 0.82f, 1.0f); break;
        case Face::RIGHT:  shade = glm::vec4(1.00f, 1.00f, 1.00f, 1.0f); break;
        case Face::FRONT:  shade = glm::vec4(0.92f, 0.92f, 0.92f, 1.0f); break;
        case Face::BACK:   shade = glm::vec4(0.86f, 0.86f, 0.86f, 1.0f); break;
    }

    rngSeed(0x9E3779B9u ^ (tileX * 0x9E3779B9u) ^ (tileY * 0xBB67AE85u)
            ^ (static_cast<std::uint32_t>(id) * 0x85EBCA77u)
            ^ (static_cast<std::uint32_t>(face) * 0xC2B2AE3Du));

    const bool isWood = (id == BLOCK_WOOD);
    const bool isLeaves = (id == BLOCK_LEAVES);
    const bool isOre = (id >= BLOCK_COAL_ORE && id <= BLOCK_DIAMOND_ORE);
    const bool isGrass = (id == BLOCK_GRASS);

    for (std::uint32_t y = 0; y < tileSize; ++y) {
        for (std::uint32_t x = 0; x < tileSize; ++x) {
            const std::uint32_t ox = tileX * tileSize + x;
            const std::uint32_t oy = tileY * tileSize + y;
            const std::uint32_t i = oy * stride + ox * 4;

            glm::vec4 color = base * shade;

            // Noise grain.
            float grain = (rngFloat() - 0.5f) * 0.12f;
            color = glm::clamp(color + glm::vec4(grain, grain, grain, 0.0f), 0.0f, 1.0f);

            // Wood grain pattern.
            if (isWood) {
                float grainY = static_cast<float>(y) / static_cast<float>(tileSize);
                float rings = std::sin(grainY * 20.0f + x * 0.3f) * 0.08f;
                color = glm::clamp(color + glm::vec4(rings, rings * 0.5f, rings * 0.2f, 0.0f), 0.0f, 1.0f);
            }

            // Leaves transparency + darker spots.
            if (isLeaves) {
                if ((x + y * 3) % 7 == 0) {
                    color *= glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
                }
            }

            // Grass side gets dirt at bottom rows.
            if (isGrass && face == Face::RIGHT && y > tileSize * 2 / 3) {
                color = blockBaseColor(BLOCK_DIRT) * shade;
                color = glm::clamp(color + glm::vec4((rngFloat()-0.5f)*0.08f, (rngFloat()-0.5f)*0.08f, 0.0f, 0.0f), 0.0f, 1.0f);
            }

            // Ore specks.
            if (isOre) {
                float speck = std::sin(x * 1.7f + y * 2.3f + rngFloat() * 10.0f)
                            * std::cos(x * 0.9f - y * 1.4f + rngFloat() * 10.0f);
                if (speck > 0.82f) {
                    color = oreSpeckColor(id) * shade;
                }
            }

            // Water is translucent with caustics.
            if (id == BLOCK_WATER) {
                float caustic = std::sin(x * 2.0f) * std::cos(y * 2.0f) * 0.15f;
                color.r = glm::clamp(color.r + caustic, 0.0f, 1.0f);
                color.b = glm::clamp(color.b + caustic * 1.5f, 0.0f, 1.0f);
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
TextureAtlas::~TextureAtlas() {
    if (m_textureID) glDeleteTextures(1, &m_textureID);
}

void TextureAtlas::buildProcedural(std::uint32_t tileSize,
                                   std::uint32_t tilesPerRow) {
    m_tileSize = tileSize;
    m_tilesPerRow = tilesPerRow;

    // Assign tile indices: 6 faces per block id.
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
