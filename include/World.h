#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>

#include <glm/glm.hpp>

#include "Camera.h"
#include "Chunk.h"
#include "Noise.h"
#include "TextureAtlas.h"

namespace voxel {

class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    std::shared_future<void> generateChunkAsync(int chunkX, int chunkZ);

    Chunk* getChunk(int chunkX, int chunkZ) const;
    Chunk* getChunkOrLoad(int chunkX, int chunkZ);

    void buildChunkMesh(const TextureAtlas& atlas, int chunkX, int chunkZ);

    BlockID getBlock(int worldX, int worldY, int worldZ) const;
    void setBlock(int worldX, int worldY, int worldZ, BlockID id);

    void collectSolidAABBs(std::vector<AABB>& out) const;

    [[nodiscard]] int heightAt(int worldX, int worldZ) const;
    [[nodiscard]] BlockID surfaceBlock(int worldX, int worldZ) const;

    const PerlinNoise& noise() const { return m_noise; }

    void markMeshDirty(int chunkX, int chunkZ);
    bool needsMesh(int chunkX, int chunkZ) const;
    void clearNeedsMesh(int chunkX, int chunkZ);

    // Save/load support.
    void forEachChunk(std::function<void(int, int, Chunk*)> fn) const;

private:
    using ChunkKey = std::int64_t;
    static inline ChunkKey makeKey(int x, int z) {
        return (static_cast<ChunkKey>(x) << 32) |
               (static_cast<ChunkKey>(static_cast<std::uint32_t>(z)));
    }

    void generateChunkBlocks(Chunk& chunk);

    std::unordered_map<ChunkKey, ChunkPtr> m_chunks;
    std::unordered_map<ChunkKey, std::shared_future<void>> m_generationFutures;
    mutable std::mutex m_mutex;

    PerlinNoise m_noise;
    std::unordered_set<ChunkKey> m_pendingMesh;
};

} // namespace voxel
