#include "Chunk.h"

#include <glad/glad.h>

#include <algorithm>

namespace voxel {

namespace {
// Face direction vectors (x, y, z).
constexpr std::array<glm::ivec3, 6> faceDirs = {{
    glm::ivec3(0, 1, 0),   // TOP
    glm::ivec3(0, -1, 0),  // BOTTOM
    glm::ivec3(-1, 0, 0),  // LEFT
    glm::ivec3(1, 0, 0),   // RIGHT
    glm::ivec3(0, 0, 1),   // FRONT
    glm::ivec3(0, 0, -1),  // BACK
}};

// Normal vectors matching Face order.
constexpr std::array<glm::vec3, 6> faceNormals = {{
    glm::vec3(0, 1, 0),   // TOP
    glm::vec3(0, -1, 0),  // BOTTOM
    glm::vec3(-1, 0, 0),  // LEFT
    glm::vec3(1, 0, 0),   // RIGHT
    glm::vec3(0, 0, 1),   // FRONT
    glm::vec3(0, 0, -1),  // BACK
}};

// Face corners in CCW order when viewed from outside the cube.
// Face-local UVs are assigned as (0,0),(1,0),(1,1),(0,1) in this same order.
constexpr std::array<std::array<glm::ivec3, 4>, 6> faceCorners = {{
    // TOP: normal (0,1,0), viewed from above
    {{glm::ivec3(0,1,0), glm::ivec3(1,1,0), glm::ivec3(1,1,1), glm::ivec3(0,1,1)}},

    // BOTTOM: normal (0,-1,0), viewed from below
    {{glm::ivec3(0,0,0), glm::ivec3(0,0,1), glm::ivec3(1,0,1), glm::ivec3(1,0,0)}},

    // LEFT: normal (-1,0,0), viewed from left (-X)
    {{glm::ivec3(0,0,0), glm::ivec3(0,1,0), glm::ivec3(0,1,1), glm::ivec3(0,0,1)}},

    // RIGHT: normal (1,0,0), viewed from right (+X)
    {{glm::ivec3(1,0,1), glm::ivec3(1,1,1), glm::ivec3(1,1,0), glm::ivec3(1,0,0)}},

    // FRONT: normal (0,0,1), viewed from front (+Z)
    {{glm::ivec3(0,0,1), glm::ivec3(1,0,1), glm::ivec3(1,1,1), glm::ivec3(0,1,1)}},

    // BACK: normal (0,0,-1), viewed from back (-Z)
    {{glm::ivec3(1,0,0), glm::ivec3(0,0,0), glm::ivec3(0,1,0), glm::ivec3(1,1,0)}},
}};

// Face-local UV coordinates matching the corner order above.
// (0,0),(1,0),(1,1),(0,1) for each face.
constexpr std::array<std::array<glm::vec2, 4>, 6> faceUVs = {{
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // TOP
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // BOTTOM
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // LEFT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // RIGHT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // FRONT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // BACK
}};
} // namespace

Chunk::Chunk(int chunkX, int chunkZ)
    : m_chunkX(chunkX), m_chunkZ(chunkZ) {
    m_blocks.fill(BLOCK_AIR);
}

Chunk::~Chunk() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

BlockID Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return BLOCK_AIR;
    return m_blocks[indexOf(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, BlockID id) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return;
    m_blocks[indexOf(x, y, z)] = id;
    setMeshDirty(true);
}

bool Chunk::isSolid(int x, int y, int z) const {
    BlockID id = getBlock(x, y, z);
    if (id == BLOCK_AIR) return false;
    return blocks().isOpaque(id);
}

void Chunk::buildMesh(const TextureAtlas& atlas,
                      std::function<bool(int x, int y, int z)> solidOutside) {
    auto solid = [&](int x, int y, int z) -> bool {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && z >= 0 && z < DEPTH)
            return isSolid(x, y, z);
        if (solidOutside) return solidOutside(x, y, z);
        return false;
    };

    if (!m_meshDirty && m_vertexCount > 0) return;

    std::vector<float> vertices;
    vertices.reserve(8192);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int z = 0; z < DEPTH; ++z) {
            for (int x = 0; x < WIDTH; ++x) {
                BlockID id = m_blocks[indexOf(x, y, z)];
                if (id == BLOCK_AIR) continue;
                const auto& props = blocks().get(id);
                if (!props.solid) continue;

                glm::vec3 base(static_cast<float>(x),
                               static_cast<float>(y),
                               static_cast<float>(z));

                // Get atlas UV info for this block's first face (all faces use same tile size)
                glm::vec4 rect = atlas.tileRect(id, Face::TOP);
                glm::vec2 atlasOrigin(rect.x, rect.y);
                float tileU = rect.z;
                float tileV = rect.w;

                for (std::uint32_t f = 0; f < 6; ++f) {
                    // Face culling: skip faces shared with an opaque neighbour.
                    glm::ivec3 n = glm::ivec3(base) + faceDirs[f];
                    if (solid(n.x, n.y, n.z)) continue;

                    glm::vec3 normal = faceNormals[f];
                    const auto& corners = faceCorners[f];
                    const auto& uvs = faceUVs[f];

                    // All 6 faces of a block use the same tile (simple coloring).
                    // For different textures per face, we'd look up tileRect per face.
                    glm::vec2 faceAtlasOrigin = atlasOrigin;

                    for (std::uint32_t c = 0; c < 4; ++c) {
                        glm::vec3 pos = base + glm::vec3(corners[c]);
                        glm::vec2 faceUV = uvs[c];
                        glm::vec2 uv = faceAtlasOrigin + faceUV * glm::vec2(tileU, tileV);

                        vertices.push_back(pos.x);
                        vertices.push_back(pos.y);
                        vertices.push_back(pos.z);
                        vertices.push_back(normal.x);
                        vertices.push_back(normal.y);
                        vertices.push_back(normal.z);
                        vertices.push_back(uv.x);
                        vertices.push_back(uv.y);
                    }
                }
            }
        }
    }

    uploadMesh(vertices);
    m_meshDirty = false;
}

void Chunk::uploadMesh(const std::vector<float>& vertices) {
    m_vertexCount = static_cast<GLsizei>(vertices.size() / 8);

    if (m_vao == 0) glGenVertexArrays(1, &m_vao);
    if (m_vbo == 0) glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (m_vertexCount > 0) {
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                     vertices.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }

    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Chunk::render() const {
    if (m_vertexCount == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}

} // namespace voxel
