#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Block.h"
#include "TextureAtlas.h"

namespace voxel {

// A chunk is a 16 x 256 x 16 block of voxels stored in a flat, data-oriented
// array for cache-friendly traversal during meshing and generation.
class Chunk {
public:
    static constexpr int WIDTH = 16;
    static constexpr int HEIGHT = 256;
    static constexpr int DEPTH = 16;
    static constexpr int SIZE = WIDTH * HEIGHT * DEPTH;

    // Chunk coordinate in world space (chunk space).
    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = default;
    Chunk& operator=(Chunk&&) = default;

    int chunkX() const { return m_chunkX; }
    int chunkZ() const { return m_chunkZ; }

    // Block access. x,y,z are local chunk coordinates in [0, WIDTH/HEIGHT/DEPTH).
    BlockID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockID id);

    // Returns true if the block at local (x,y,z) is solid according to the
    // block registry. Out-of-bounds queries return false (air).
    bool isSolid(int x, int y, int z) const;

    // Build the OpenGL mesh (VBO/VAO) from the current block data using
    // face culling: internal faces shared by two opaque neighbours are
    // discarded. `atlas` supplies the per-face texture UVs. `solidOutside`
    // lets the caller report whether a block in a neighbouring chunk is solid
    // (default: out-of-chunk = air).
    void buildMesh(const TextureAtlas& atlas,
                   std::function<bool(int x, int y, int z)> solidOutside = nullptr);

    void render() const;

    bool hasMesh() const { return m_vertexCount > 0; }
    bool meshDirty() const { return m_meshDirty; }
    void setMeshDirty(bool dirty = true) { m_meshDirty = dirty; }

    // World-space position of the chunk's origin.
    glm::vec3 origin() const {
        return glm::vec3(static_cast<float>(m_chunkX) * WIDTH, 0.0f,
                         static_cast<float>(m_chunkZ) * DEPTH);
    }

private:
    int m_chunkX = 0;
    int m_chunkZ = 0;

    // Flat block storage: index = y * (WIDTH * DEPTH) + z * WIDTH + x.
    std::array<BlockID, SIZE> m_blocks{};

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;
    bool m_meshDirty = true;

    static inline int indexOf(int x, int y, int z) {
        return y * (WIDTH * DEPTH) + z * WIDTH + x;
    }

    void uploadMesh(const std::vector<float>& vertices);
};

using ChunkPtr = std::unique_ptr<Chunk>;

} // namespace voxel
