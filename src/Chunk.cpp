#include "Chunk.h"
#include "TextureAtlas.h"

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

// For each face, the four corner offsets (x, y, z) forming a unit quad.
constexpr std::array<std::array<glm::ivec3, 4>, 6> faceCorners = {{
    // TOP:     y = +1 face
    {{{0,1,0},{1,1,0},{1,1,1},{0,1,1}}},
    // BOTTOM:  y = 0 face
    {{{0,0,0},{0,0,1},{1,0,1},{1,0,0}}},
    // LEFT:    x = 0 face
    {{{0,0,0},{0,0,1},{0,1,1},{0,1,0}}},
    // RIGHT:   x = +1 face
    {{{1,0,1},{1,0,0},{1,1,0},{1,1,1}}},
    // FRONT:   z = +1 face
    {{{0,0,1},{1,0,1},{1,1,1},{0,1,1}}},
    // BACK:    z = 0 face
    {{{1,0,0},{0,0,0},{0,1,0},{1,1,0}}},
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
    // Resolve neighbour-solidity: prefer the chunk's own blocks, fall back to
    // the caller-provided callback for out-of-chunk queries, else air.
    auto solid = [&](int x, int y, int z) -> bool {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && z >= 0 && z < DEPTH)
            return isSolid(x, y, z);
        if (solidOutside) return solidOutside(x, y, z);
        return false;
    };

    if (!m_meshDirty && m_vertexCount > 0) return;

    std::vector<float> vertices;
    vertices.reserve(4096); // typical chunk face count upper bound hint

    for (int y = 0; y < HEIGHT; ++y) {
        for (int z = 0; z < DEPTH; ++z) {
            for (int x = 0; x < WIDTH; ++x) {
                BlockID id = m_blocks[indexOf(x, y, z)];
                if (id == BLOCK_AIR) continue;
                const auto& props = blocks().get(id);
                if (!props.solid) continue;

                const glm::vec3 base(static_cast<float>(x),
                                     static_cast<float>(y),
                                     static_cast<float>(z));

                for (std::uint32_t f = 0; f < 6; ++f) {
                    // Face culling: skip faces shared with an opaque neighbour.
                    glm::ivec3 n = glm::ivec3(base) + faceDirs[f];
                    if (solid(n.x, n.y, n.z)) continue;

                    // Transparent neighbours (e.g. leaves) do not cull the face.
                    glm::vec3 normal = faceNormals[f];
                    const auto& corners = faceCorners[f];

                    // Texture UV for this face, taken from the atlas tile rect.
                    glm::vec4 rect = atlas.tileRect(id, static_cast<Face>(f));
                    glm::vec2 uvOrigin(rect.x, rect.y);
                    float tw = rect.z;
                    float th = rect.w;

                    // Corner UV offsets in atlas space.
                    for (std::uint32_t c = 0; c < 4; ++c) {
                        glm::vec3 pos = base + glm::vec3(corners[c]);
                        // Face-aligned UV: choose which axis pair to flatten.
                        glm::vec2 uv;
                        switch (static_cast<Face>(f)) {
                            case Face::TOP:    uv = glm::vec2(corners[c].x, corners[c].z); break;
                            case Face::BOTTOM: uv = glm::vec2(corners[c].x, corners[c].z); break;
                            case Face::LEFT:   uv = glm::vec2(corners[c].z, corners[c].y); break;
                            case Face::RIGHT:  uv = glm::vec2(corners[c].z, corners[c].y); break;
                            case Face::FRONT:  uv = glm::vec2(corners[c].x, corners[c].y); break;
                            case Face::BACK:   uv = glm::vec2(corners[c].x, corners[c].y); break;
                        }
                        uv = uvOrigin + uv * glm::vec2(tw, th);

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
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2); // uv
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
