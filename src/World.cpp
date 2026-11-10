#include "World.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace voxel {

World::World() = default;
World::~World() = default;

void World::generateChunkBlocks(Chunk& chunk) {
    const int cx = chunk.chunkX() * Chunk::WIDTH;
    const int cz = chunk.chunkZ() * Chunk::DEPTH;

    for (int z = 0; z < Chunk::DEPTH; ++z) {
        for (int x = 0; x < Chunk::WIDTH; ++x) {
            const int wx = cx + x;
            const int wz = cz + z;

            double h = 0.0;
            h += m_noise.fbm2D(wx * 0.007, wz * 0.007, 5, 2.1, 0.55);
            h += 0.5 * m_noise.fbm2D(wx * 0.03, wz * 0.03, 3, 2.0, 0.5);
            h = std::clamp(h, -1.0, 1.0);

            int surface = static_cast<int>(66.0 + h * 40.0);
            surface = std::clamp(surface, 4, Chunk::HEIGHT - 4);

            double cave = m_noise.fbm3D(wx * 0.15,
                                        static_cast<double>(surface) * 0.15,
                                        wz * 0.15, 3, 2.0, 0.5);
            int caveFloor = surface - static_cast<int>((cave + 1.0) * 1.5);

            for (int y = 0; y < Chunk::HEIGHT; ++y) {
                BlockID id = BLOCK_AIR;

                if (y <= surface) {
                    id = BLOCK_STONE;
                    if (y < caveFloor && cave > 0.45) id = BLOCK_AIR;
                    if (surface <= 64 && y > surface - 2) id = BLOCK_SAND;
                }

                if (id == BLOCK_STONE || id == BLOCK_SAND) {
                    int depthBelow = surface - y;
                    if (depthBelow == 0) {
                        id = (surface <= 64) ? BLOCK_SAND : BLOCK_GRASS;
                    } else if (depthBelow <= 3) {
                        id = BLOCK_DIRT;
                    }
                }

                chunk.setBlock(x, y, z, id);
            }
        }
    }
}

std::shared_future<void> World::generateChunkAsync(int chunkX, int chunkZ) {
    ChunkKey key = makeKey(chunkX, chunkZ);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_chunks.find(key) != m_chunks.end()) return m_generationFutures[key];
    if (m_generationFutures.find(key) != m_generationFutures.end()) {
        return m_generationFutures[key];
    }

    auto chunk = std::make_unique<Chunk>(chunkX, chunkZ);
    Chunk* raw = chunk.get();

    std::future<void> fut = std::async(std::launch::async, [this, raw]() {
        generateChunkBlocks(*raw);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingMesh.insert(makeKey(raw->chunkX(), raw->chunkZ()));
        }
    });

    m_chunks.emplace(key, std::move(chunk));
    auto shared = fut.share();
    m_generationFutures.emplace(key, shared);

    return shared;
}

Chunk* World::getChunk(int chunkX, int chunkZ) const {
    ChunkKey key = makeKey(chunkX, chunkZ);
    auto it = m_chunks.find(key);
    return it == m_chunks.end() ? nullptr : it->second.get();
}

Chunk* World::getChunkOrLoad(int chunkX, int chunkZ) {
    if (Chunk* c = getChunk(chunkX, chunkZ)) return c;
    generateChunkAsync(chunkX, chunkZ);
    return getChunk(chunkX, chunkZ);
}

BlockID World::getBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= Chunk::HEIGHT) return BLOCK_AIR;

    int cx = worldX >= 0 ? worldX / Chunk::WIDTH
                         : (worldX - Chunk::WIDTH + 1) / Chunk::WIDTH;
    int cz = worldZ >= 0 ? worldZ / Chunk::DEPTH
                         : (worldZ - Chunk::DEPTH + 1) / Chunk::DEPTH;
    int lx = worldX - cx * Chunk::WIDTH;
    int lz = worldZ - cz * Chunk::DEPTH;

    Chunk* c = getChunk(cx, cz);
    return c ? c->getBlock(lx, worldY, lz) : BLOCK_AIR;
}

void World::setBlock(int worldX, int worldY, int worldZ, BlockID id) {
    if (worldY < 0 || worldY >= Chunk::HEIGHT) return;
    int cx = worldX >= 0 ? worldX / Chunk::WIDTH
                         : (worldX - Chunk::WIDTH + 1) / Chunk::WIDTH;
    int cz = worldZ >= 0 ? worldZ / Chunk::DEPTH
                         : (worldZ - Chunk::DEPTH + 1) / Chunk::DEPTH;
    int lx = worldX - cx * Chunk::WIDTH;
    int lz = worldZ - cz * Chunk::DEPTH;

    Chunk* c = getChunk(cx, cz);
    if (c) {
        c->setBlock(lx, worldY, lz, id);
        c->setMeshDirty(true);
    }
}

void World::buildChunkMesh(const TextureAtlas& atlas, int chunkX, int chunkZ) {
    Chunk* c = getChunk(chunkX, chunkZ);
    if (!c) return;

    auto solidOutside = [this](int x, int y, int z) -> bool {
        (void)x; (void)y; (void)z;
        return false;
    };
    c->buildMesh(atlas, solidOutside);
    clearNeedsMesh(chunkX, chunkZ);
}

void World::collectSolidAABBs(std::vector<AABB>& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    out.clear();
    out.reserve(8192);

    for (const auto& [key, chunk] : m_chunks) {
        int cx = static_cast<int>(key >> 32);
        int cz = static_cast<int>(static_cast<std::uint32_t>(key));
        float ox = static_cast<float>(cx * Chunk::WIDTH);
        float oz = static_cast<float>(cz * Chunk::DEPTH);

        for (int y = 0; y < Chunk::HEIGHT; ++y) {
            for (int z = 0; z < Chunk::DEPTH; ++z) {
                for (int x = 0; x < Chunk::WIDTH; ++x) {
                    BlockID id = chunk->getBlock(x, y, z);
                    if (id == BLOCK_AIR) continue;
                    const auto& props = blocks().get(id);
                    if (!props.solid) continue;

                    AABB box{
                        glm::vec3(ox + x, static_cast<float>(y), oz + z),
                        glm::vec3(ox + x + 1, static_cast<float>(y + 1), oz + z + 1)
                    };
                    out.push_back(box);
                }
            }
        }
    }
}

int World::heightAt(int worldX, int worldZ) const {
    int cx = worldX >= 0 ? worldX / Chunk::WIDTH
                         : (worldX - Chunk::WIDTH + 1) / Chunk::WIDTH;
    int cz = worldZ >= 0 ? worldZ / Chunk::DEPTH
                         : (worldZ - Chunk::DEPTH + 1) / Chunk::DEPTH;
    int lx = worldX - cx * Chunk::WIDTH;
    int lz = worldZ - cz * Chunk::DEPTH;

    Chunk* c = getChunk(cx, cz);
    if (!c) return 66;

    for (int y = Chunk::HEIGHT - 1; y >= 0; --y) {
        if (c->getBlock(lx, y, lz) != BLOCK_AIR) return y;
    }
    return 0;
}

BlockID World::surfaceBlock(int worldX, int worldZ) const {
    int h = heightAt(worldX, worldZ);
    int cx = worldX >= 0 ? worldX / Chunk::WIDTH
                         : (worldX - Chunk::WIDTH + 1) / Chunk::WIDTH;
    int cz = worldZ >= 0 ? worldZ / Chunk::DEPTH
                         : (worldZ - Chunk::DEPTH + 1) / Chunk::DEPTH;
    int lx = worldX - cx * Chunk::WIDTH;
    int lz = worldZ - cz * Chunk::DEPTH;
    Chunk* c = getChunk(cx, cz);
    return c ? c->getBlock(lx, h, lz) : BLOCK_AIR;
}

void World::markMeshDirty(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingMesh.insert(makeKey(chunkX, chunkZ));
}

bool World::needsMesh(int chunkX, int chunkZ) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingMesh.contains(makeKey(chunkX, chunkZ));
}

void World::clearNeedsMesh(int chunkX, int chunkZ) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingMesh.erase(makeKey(chunkX, chunkZ));
}

void World::forEachChunk(std::function<void(int, int, Chunk*)> fn) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [key, chunk] : m_chunks) {
        int cx = static_cast<int>(key >> 32);
        int cz = static_cast<int>(static_cast<std::uint32_t>(key));
        fn(cx, cz, chunk.get());
    }
}

} // namespace voxel
