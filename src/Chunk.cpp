#include "Chunk.h"
#include "TextureAtlas.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstring>
#include <vector>

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

// For each face, the corner offsets relative to face origin.
// Each quad is drawn as CCW when viewed from outside.
constexpr std::array<std::array<glm::ivec3, 4>, 6> faceCorners = {{
    // TOP:    y=+1
    {{glm::ivec3(0,1,0), glm::ivec3(1,1,0), glm::ivec3(1,1,1), glm::ivec3(0,1,1)}},
    // BOTTOM: y=0
    {{glm::ivec3(0,0,0), glm::ivec3(0,0,1), glm::ivec3(1,0,1), glm::ivec3(1,0,0)}},
    // LEFT:   x=0
    {{glm::ivec3(0,0,0), glm::ivec3(0,1,0), glm::ivec3(0,1,1), glm::ivec3(0,0,1)}},
    // RIGHT:  x=+1
    {{glm::ivec3(1,0,1), glm::ivec3(1,1,1), glm::ivec3(1,1,0), glm::ivec3(1,0,0)}},
    // FRONT:  z=+1
    {{glm::ivec3(0,0,1), glm::ivec3(1,0,1), glm::ivec3(1,1,1), glm::ivec3(0,1,1)}},
    // BACK:   z=0
    {{glm::ivec3(1,0,0), glm::ivec3(0,0,0), glm::ivec3(0,1,0), glm::ivec3(1,1,0)}},
}};

// UV offsets for each corner of a quad.
constexpr std::array<std::array<glm::vec2, 4>, 6> cornerUVOffsets = {{
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // TOP
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // BOTTOM
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // LEFT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // RIGHT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // FRONT
    {{glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)}},  // BACK
}};

// Check if two blocks have the same texture for a given face direction.
// Greedy meshing only merges faces with identical appearance.
inline bool sameTexture(int x1, int y1, int z1, int x2, int y2, int z2,
                        const Chunk& chunk) {
    BlockID id1 = chunk.getBlock(x1, y1, z1);
    BlockID id2 = chunk.getBlock(x2, y2, z2);
    return id1 == id2;
}
} // namespace

Chunk::Chunk(int chunkX, int chunkZ)
    : m_chunkX(chunkX), m_chunkZ(chunkZ) {
    m_blocks.fill(BLOCK_AIR);
}

Chunk::~Chunk() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
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
    std::vector<unsigned int> indices;

    // Greedy meshing: for each face direction, scan the 2D plane and merge
    // adjacent coplanar faces with the same texture into large rectangles.
    for (std::uint32_t f = 0; f < 6; ++f) {
        const auto& normal = faceNormals[f];

        // Dimensions of the 2D plane for this face direction.
        int dim1, dim2;
        switch (static_cast<Face>(f)) {
            case Face::TOP:    dim1 = WIDTH;  dim2 = DEPTH; break;
            case Face::BOTTOM: dim1 = WIDTH;  dim2 = DEPTH; break;
            case Face::LEFT:   dim1 = DEPTH; dim2 = HEIGHT; break;
            case Face::RIGHT:  dim1 = DEPTH; dim2 = HEIGHT; break;
            case Face::FRONT:  dim1 = WIDTH; dim2 = HEIGHT; break;
            case Face::BACK:   dim1 = WIDTH; dim2 = HEIGHT; break;
        }

        // visited[d1][d2] tracks if this cell in the 2D plane is already merged.
        std::vector<std::vector<bool>> visited(dim1,
            std::vector<bool>(dim2, false));

        // The fixed coordinate along dim3 for this face.
        int fixedCoord = (f == static_cast<std::uint32_t>(Face::TOP)) ? HEIGHT - 1 : 0;
        if (f == static_cast<std::uint32_t>(Face::BOTTOM)) fixedCoord = 0;
        if (f == static_cast<std::uint32_t>(Face::LEFT)) fixedCoord = 0;
        if (f == static_cast<std::uint32_t>(Face::RIGHT)) fixedCoord = WIDTH - 1;
        if (f == static_cast<std::uint32_t>(Face::FRONT)) fixedCoord = DEPTH - 1;
        if (f == static_cast<std::uint32_t>(Face::BACK)) fixedCoord = 0;

        for (int d1 = 0; d1 < dim1; ++d1) {
            for (int d2 = 0; d2 < dim2; ++d2) {
                if (visited[d1][d2]) continue;

                // Convert 2D plane coords to 3D world coords.
                int x, y, z;
                switch (static_cast<Face>(f)) {
                    case Face::TOP:    x = d1; y = fixedCoord; z = d2; break;
                    case Face::BOTTOM: x = d1; y = fixedCoord; z = d2; break;
                    case Face::LEFT:   x = fixedCoord; y = d2; z = d1; break;
                    case Face::RIGHT:  x = fixedCoord; y = d2; z = d1; break;
                    case Face::FRONT:  x = d1; y = d2; z = fixedCoord; break;
                    case Face::BACK:   x = d1; y = d2; z = fixedCoord; break;
                }

                // Check if this face should be rendered (solid block, air neighbor).
                if (!solid(x, y, z)) {
                    visited[d1][d2] = true;
                    continue;
                }

                int neighborX = x + faceDirs[f].x;
                int neighborY = y + faceDirs[f].y;
                int neighborZ = z + faceDirs[f].z;
                if (solid(neighborX, neighborY, neighborZ)) {
                    visited[d1][d2] = true;
                    continue;
                }

                // Get the texture for this face.
                BlockID blockId = m_blocks[indexOf(x, y, z)];
                int faceTex = blocks().getFaceTexture(blockId, static_cast<Face>(f));

                // Greedy merge: find max width along axis1.
                int width = 1;
                while (d1 + width < dim1) {
                    int nx, ny, nz;
                    switch (static_cast<Face>(f)) {
                        case Face::TOP:    nx = d1 + width; ny = fixedCoord; nz = d2; break;
                        case Face::BOTTOM: nx = d1 + width; ny = fixedCoord; nz = d2; break;
                        case Face::LEFT:   nx = fixedCoord; ny = d2; nz = d1 + width; break;
                        case Face::RIGHT:  nx = fixedCoord; ny = d2; nz = d1 + width; break;
                        case Face::FRONT:  nx = d1 + width; ny = d2; nz = fixedCoord; break;
                        case Face::BACK:   nx = d1 + width; ny = d2; nz = fixedCoord; break;
                    }
                    if (!solid(nx, ny, nz)) break;
                    int nnx = nx + faceDirs[f].x;
                    int nny = ny + faceDirs[f].y;
                    int nnz = nz + faceDirs[f].z;
                    if (solid(nnx, nny, nnz)) break;
                    BlockID nid = m_blocks[indexOf(nx, ny, nz)];
                    if (blocks().getFaceTexture(nid, static_cast<Face>(f)) != faceTex) break;
                    ++width;
                }

                // Find max height along axis2 for the current width.
                int height = 1;
                bool canExpand = true;
                while (d2 + height < dim2 && canExpand) {
                    for (int w = 0; w < width; ++w) {
                        int nx, ny, nz;
                        switch (static_cast<Face>(f)) {
                            case Face::TOP:    nx = d1 + w; ny = fixedCoord; nz = d2 + height; break;
                            case Face::BOTTOM: nx = d1 + w; ny = fixedCoord; nz = d2 + height; break;
                            case Face::LEFT:   nx = fixedCoord; ny = d2 + height; nz = d1 + w; break;
                            case Face::RIGHT:  nx = fixedCoord; ny = d2 + height; nz = d1 + w; break;
                            case Face::FRONT:  nx = d1 + w; ny = d2 + height; nz = fixedCoord; break;
                            case Face::BACK:   nx = d1 + w; ny = d2 + height; nz = fixedCoord; break;
                        }
                        if (!solid(nx, ny, nz)) { canExpand = false; break; }
                        int nnx = nx + faceDirs[f].x;
                        int nny = ny + faceDirs[f].y;
                        int nnz = nz + faceDirs[f].z;
                        if (solid(nnx, nny, nnz)) { canExpand = false; break; }
                        BlockID nid = m_blocks[indexOf(nx, ny, nz)];
                        if (blocks().getFaceTexture(nid, static_cast<Face>(f)) != faceTex) { canExpand = false; break; }
                    }
                    if (canExpand) ++height;
                }

                // Mark all cells in this rectangle as visited.
                for (int w = 0; w < width; ++w) {
                    for (int h = 0; h < height; ++h) {
                        int vd1, vd2;
                        switch (static_cast<Face>(f)) {
                            case Face::TOP:    vd1 = d1 + w; vd2 = d2 + h; break;
                            case Face::BOTTOM: vd1 = d1 + w; vd2 = d2 + h; break;
                            case Face::LEFT:   vd1 = d1 + w; vd2 = d2 + h; break;
                            case Face::RIGHT:  vd1 = d1 + w; vd2 = d2 + h; break;
                            case Face::FRONT:  vd1 = d1 + w; vd2 = d2 + h; break;
                            case Face::BACK:   vd1 = d1 + w; vd2 = d2 + h; break;
                        }
                        visited[vd1][vd2] = true;
                    }
                }

                // Emit the quad.
                int baseIndex = static_cast<int>(vertices.size() / 8);

                // Convert 2D plane dims to 3D corner positions.
                auto planeTo3D = [&](int pd1, int pd2) -> glm::ivec3 {
                    int px, py, pz;
                    switch (static_cast<Face>(f)) {
                        case Face::TOP:    px = pd1; py = fixedCoord; pz = pd2; break;
                        case Face::BOTTOM: px = pd1; py = fixedCoord; pz = pd2; break;
                        case Face::LEFT:   px = fixedCoord; py = pd2; pz = pd1; break;
                        case Face::RIGHT:  px = fixedCoord; py = pd2; pz = pd1; break;
                        case Face::FRONT:  px = pd1; py = pd2; pz = fixedCoord; break;
                        case Face::BACK:   px = pd1; py = pd2; pz = fixedCoord; break;
                    }
                    return glm::ivec3(px, py, pz);
                };

                glm::vec3 c00 = planeTo3D(d1, d2);
                glm::vec3 c10 = planeTo3D(d1 + width - 1, d2);
                glm::vec3 c11 = planeTo3D(d1 + width - 1, d2 + height - 1);
                glm::vec3 c01 = planeTo3D(d1, d2 + height - 1);

                // Get texture atlas UV rect.
                glm::vec4 texRect = atlas.tileRect(blockId, static_cast<Face>(f));
                glm::vec2 texOrigin(texRect.x, texRect.y);
                glm::vec2 texSize(texRect.z, texRect.w);

                // Scale UVs by the quad size (so texture stretches across the quad).
                glm::vec2 uvScale(width, height);

                // Emit 4 vertices (CCW when viewed from outside).
                for (int c = 0; c < 4; ++c) {
                    glm::ivec3 corner;
                    glm::vec2 uvOffset;
                    switch (c) {
                        case 0: corner = c00; uvOffset = glm::vec2(0, 0); break;
                        case 1: corner = c10; uvOffset = glm::vec2(1, 0); break;
                        case 2: corner = c11; uvOffset = glm::vec2(1, 1); break;
                        case 3: corner = c01; uvOffset = glm::vec2(0, 1); break;
                    }

                    glm::vec2 uv = texOrigin + uvOffset * texSize;

                    vertices.push_back(static_cast<float>(corner.x));
                    vertices.push_back(static_cast<float>(corner.y));
                    vertices.push_back(static_cast<float>(corner.z));
                    vertices.push_back(normal.x);
                    vertices.push_back(normal.y);
                    vertices.push_back(normal.z);
                    vertices.push_back(uv.x);
                    vertices.push_back(uv.y);
                }

                // Two triangles per quad.
                indices.push_back(static_cast<unsigned int>(baseIndex + 0));
                indices.push_back(static_cast<unsigned int>(baseIndex + 1));
                indices.push_back(static_cast<unsigned int>(baseIndex + 2));
                indices.push_back(static_cast<unsigned int>(baseIndex + 0));
                indices.push_back(static_cast<unsigned int>(baseIndex + 2));
                indices.push_back(static_cast<unsigned int>(baseIndex + 3));
            }
        }
    }

    uploadMesh(vertices, indices);
    m_meshDirty = false;
}

void Chunk::uploadMesh(const std::vector<float>& vertices,
                       const std::vector<unsigned int>& indices) {
    m_vertexCount = static_cast<GLsizei>(vertices.size() / 8);
    m_indexCount = static_cast<GLsizei>(indices.size());

    if (m_vao == 0) glGenVertexArrays(1, &m_vao);
    if (m_vbo == 0) glGenBuffers(1, &m_vbo);
    if (m_ebo == 0) glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (m_vertexCount > 0) {
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                     vertices.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    if (m_indexCount > 0) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                     indices.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }

    GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);  // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);  // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);  // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(6 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Chunk::render() const {
    if (m_indexCount == 0) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace voxel
