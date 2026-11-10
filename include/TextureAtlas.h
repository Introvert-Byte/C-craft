#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

#include "Block.h"

namespace voxel {

// A texture atlas packs every block face variant into a single OpenGL texture
// so all blocks can be drawn with one draw call / texture bind.
//
// Block face variants:
//   TOP / BOTTOM / LEFT / RIGHT / FRONT / BACK
//
// The atlas is built as a grid of square tiles. Each block type reserves one
// tile per face direction (six tiles per opaque block). `tileUV()` returns the
// upper-left corner UV of the tile for a given (block, face).
class TextureAtlas {
public:
    TextureAtlas();

    // Build a procedural (no external image loader) atlas. Each tile is
    // filled with a distinct, recognisable colour/pattern so the engine is
    // self-contained and runs without any image files.
    void buildProcedural(std::uint32_t tileSize = 16,
                         std::uint32_t tilesPerRow = 16);

    // Load an atlas from an image file (stb_image-style path reserved for the
    // project's texture pack). Falls back to procedural if the file is missing.
    bool loadFromFile(const char* path, std::uint32_t tileSize = 16);

    GLuint textureID() const { return m_textureID; }
    std::uint32_t tileSize() const { return m_tileSize; }
    std::uint32_t tilesPerRow() const { return m_tilesPerRow; }
    std::uint32_t atlasWidth() const { return m_atlasWidth; }
    std::uint32_t atlasHeight() const { return m_atlasHeight; }

    // Upper-left UV corner of the tile for this block's face.
    glm::vec2 tileUV(BlockID id, Face face) const;

    // Full UV rect (min, max) for a face quad in atlas space.
    glm::vec4 tileRect(BlockID id, Face face) const;

    void bind(GLuint unit = 0) const;

private:
    GLuint m_textureID = 0;
    std::uint32_t m_tileSize = 16;
    std::uint32_t m_tilesPerRow = 16;
    std::uint32_t m_atlasWidth = 0;
    std::uint32_t m_atlasHeight = 0;
    // Tile index per (block, face): id * 6 + face.
    std::uint32_t m_tileIndex[256][6];
};

} // namespace voxel
