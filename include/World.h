#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Camera.h"
#include "Chunk.h"
#include "Noise.h"
#include "TextureAtlas.h"

namespace voxel {

// World holds all loaded chunks and drives multi-threaded chunk generation
// using std::async so the main render thread never stutters.
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Generate terrain for a chunk in a background task. Returns a shared
    // future that resolves once the chunk's blocks are populated.
    std::shared_future<void> generateChunkAsync(int chunkX, int chunkZ);

    // Ensure a chunk exists, generating it if needed. Returns the chunk.
    Chunk* getChunk(int chunkX, int chunkZ) const;
    Chunk* getChunkOrLoad(int chunkX, int chunkZ);

    // Build (or rebuild) a chunk's mesh on the calling thread using the
    // supplied texture atlas for face UVs.
    void buildChunkMesh(const TextureAtlas& atlas, int chunkX, int chunkZ);

    // Iterate all loaded chunks (for saving). `fn` receives (cx, cz, Chunk*).
    void forEachChunk(std::function<void(int, int, Chunk*)> fn) const;

    // Block read across chunk boundaries.
    BlockID getBlock(int worldX, int worldY, int worldZ) const;
    void setBlock(int worldX, int worldY, int worldZ, BlockID id);

    // All solid AABBs in the loaded chunks, for player collision.
    void collectSolidAABBs(std::vector<AABB>& out) const;

    // Terrain helpers.
    [[nodiscard]] int heightAt(int worldX, int worldZ) const;
    [[nodiscard]] BlockID surfaceBlock(int worldX, int worldZ) const;

    const PerlinNoise& noise() const { return m_noise; }

    // Track which chunks still need meshing after generation.
    void markMeshDirty(int chunkX, int chunkZ);
    bool needsMesh(int chunkX, int chunkZ) const;
    void clearNeedsMesh(int chunkX, int chunkZ);

private:
    using ChunkKey = std::int64_t;
    static inline ChunkKey makeKey(int x, int z) {
        return (static_cast<ChunkKey>(x) << 32) |
               (static_cast<ChunkKey>(static_cast<std::uint32_t>(z)));
    }

    void generateChunkBlocks(Chunk& chunk);

    // Soil layers based on depth below surface.
    static BlockID layerForDepth(int depthBelowSurface);

    std::unordered_map<ChunkKey, ChunkPtr> m_chunks;
    std::unordered_map<ChunkKey, std::shared_future<void>> m_generationFutures;
    mutable std::mutex m_mutex;

    PerlinNoise m_noise;

    std::unordered_set<ChunkKey> m_pendingMesh;
};

} // namespace voxel
