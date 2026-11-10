#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>

#include "Block.h"

namespace voxel {

class TextureAtlas {
public:
    TextureAtlas();

    void buildProcedural(std::uint32_t tileSize = 16,
                         std::uint32_t tilesPerRow = 16);

    GLuint textureID() const { return m_textureID; }
    std::uint32_t tileSize() const { return m_tileSize; }
    std::uint32_t tilesPerRow() const { return m_tilesPerRow; }
    std::uint32_t atlasWidth() const { return m_atlasWidth; }
    std::uint32_t atlasHeight() const { return m_atlasHeight; }

    glm::vec2 tileUV(BlockID id, Face face) const;
    glm::vec4 tileRect(BlockID id, Face face) const;

    void bind(GLuint unit = 0) const;

private:
    GLuint m_textureID = 0;
    std::uint32_t m_tileSize = 16;
    std::uint32_t m_tilesPerRow = 16;
    std::uint32_t m_atlasWidth = 0;
    std::uint32_t m_atlasHeight = 0;
    std::uint32_t m_tileIndex[256][6];
};

} // namespace voxel
