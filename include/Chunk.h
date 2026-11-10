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

class Chunk {
public:
    static constexpr int WIDTH = 16;
    static constexpr int HEIGHT = 256;
    static constexpr int DEPTH = 16;
    static constexpr int SIZE = WIDTH * HEIGHT * DEPTH;

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = default;
    Chunk& operator=(Chunk&&) = default;

    int chunkX() const { return m_chunkX; }
    int chunkZ() const { return m_chunkZ; }

    BlockID getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockID id);

    bool isSolid(int x, int y, int z) const;

    void buildMesh(const TextureAtlas& atlas,
                   std::function<bool(int x, int y, int z)> solidOutside = nullptr);

    void render() const;

    bool hasMesh() const { return m_vertexCount > 0; }
    bool meshDirty() const { return m_meshDirty; }
    void setMeshDirty(bool dirty = true) { m_meshDirty = dirty; }

    [[nodiscard]] GLsizei vertexCount() const { return m_vertexCount; }

    glm::vec3 origin() const {
        return glm::vec3(static_cast<float>(m_chunkX) * WIDTH, 0.0f,
                         static_cast<float>(m_chunkZ) * DEPTH);
    }

private:
    int m_chunkX = 0;
    int m_chunkZ = 0;
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
