#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <string_view>

namespace voxel {

// Unique identifier for a block type.
using BlockID = std::uint8_t;

constexpr BlockID BLOCK_AIR = 0;
constexpr BlockID BLOCK_GRASS = 1;
constexpr BlockID BLOCK_DIRT = 2;
constexpr BlockID BLOCK_STONE = 3;
constexpr BlockID BLOCK_SAND = 4;
constexpr BlockID BLOCK_WOOD = 5;
constexpr BlockID BLOCK_LEAVES = 6;

// Face directions used for meshing and UV lookup.
enum class Face : std::uint8_t {
    TOP = 0,
    BOTTOM = 1,
    LEFT = 2,
    RIGHT = 3,
    FRONT = 4,
    BACK = 5,
    COUNT = 6
};

// Block metadata: visual + physical properties of a block type.
struct BlockProperties {
    BlockID id = BLOCK_AIR;
    std::string_view name = "air";
    bool solid = false;        // collides with the player / blocks light
    bool opaque = false;       // fully opaque (neighbour faces can be culled)
    bool fullBright = false;   // emits light / unaffected by shading
};

// Registry of block definitions. Lookup by BlockID.
class BlockRegistry {
public:
    BlockRegistry();

    const BlockProperties& get(BlockID id) const;
    bool isSolid(BlockID id) const;
    bool isOpaque(BlockID id) const;

private:
    BlockProperties m_entries[256];
};

// Global registry accessor.
const BlockRegistry& blocks();

} // namespace voxel
