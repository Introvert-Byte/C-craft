#pragma once

#include <cstdint>
#include <string_view>

namespace voxel {

using BlockID = std::uint8_t;

constexpr BlockID BLOCK_AIR = 0;
constexpr BlockID BLOCK_GRASS = 1;
constexpr BlockID BLOCK_DIRT = 2;
constexpr BlockID BLOCK_STONE = 3;
constexpr BlockID BLOCK_SAND = 4;
constexpr BlockID BLOCK_WOOD = 5;
constexpr BlockID BLOCK_LEAVES = 6;

enum class Face : std::uint8_t {
    TOP = 0,
    BOTTOM = 1,
    LEFT = 2,
    RIGHT = 3,
    FRONT = 4,
    BACK = 5,
    COUNT = 6
};

struct BlockProperties {
    BlockID id = BLOCK_AIR;
    std::string_view name = "air";
    bool solid = false;
    bool opaque = false;
};

class BlockRegistry {
public:
    BlockRegistry();
    const BlockProperties& get(BlockID id) const;
    bool isSolid(BlockID id) const;
    bool isOpaque(BlockID id) const;

private:
    BlockProperties m_entries[256];
};

const BlockRegistry& blocks();

} // namespace voxel
