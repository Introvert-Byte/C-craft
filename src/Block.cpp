#include "Block.h"

namespace voxel {

const BlockRegistry& blocks() {
    static const BlockRegistry registry;
    return registry;
}

BlockRegistry::BlockRegistry() {
    // Air
    m_entries[BLOCK_AIR] = {BLOCK_AIR, "air", false, false, {-1, -1, -1, -1, -1, -1}};

    // Grass: top=grass, sides=dirt, bottom=dirt
    m_entries[BLOCK_GRASS] = {
        BLOCK_GRASS, "grass", true, true,
        {0, 1, 1, 1, 1, 1}  // TOP=grass, others=dirt
    };

    // Dirt
    m_entries[BLOCK_DIRT] = {
        BLOCK_DIRT, "dirt", true, true,
        {1, 1, 1, 1, 1, 1}  // all dirt
    };

    // Stone
    m_entries[BLOCK_STONE] = {
        BLOCK_STONE, "stone", true, true,
        {2, 2, 2, 2, 2, 2}  // all stone
    };

    // Sand
    m_entries[BLOCK_SAND] = {
        BLOCK_SAND, "sand", true, true,
        {3, 3, 3, 3, 3, 3}  // all sand
    };

    // Wood: top=log top, sides=log bark, bottom=log top
    m_entries[BLOCK_WOOD] = {
        BLOCK_WOOD, "wood", true, true,
        {4, 5, 5, 5, 5, 5}  // TOP=log_top, others=log_bark
    };

    // Leaves
    m_entries[BLOCK_LEAVES] = {
        BLOCK_LEAVES, "leaves", true, false,
        {6, 6, 6, 6, 6, 6}  // all leaves
    };

    // Water
    m_entries[BLOCK_WATER] = {
        BLOCK_WATER, "water", false, false,
        {7, 7, 7, 7, 7, 7}  // all water
    };

    // Coal ore
    m_entries[BLOCK_COAL_ORE] = {
        BLOCK_COAL_ORE, "coal_ore", true, true,
        {8, 8, 8, 8, 8, 8}
    };

    // Iron ore
    m_entries[BLOCK_IRON_ORE] = {
        BLOCK_IRON_ORE, "iron_ore", true, true,
        {9, 9, 9, 9, 9, 9}
    };

    // Gold ore
    m_entries[BLOCK_GOLD_ORE] = {
        BLOCK_GOLD_ORE, "gold_ore", true, true,
        {10, 10, 10, 10, 10, 10}
    };

    // Diamond ore
    m_entries[BLOCK_DIAMOND_ORE] = {
        BLOCK_DIAMOND_ORE, "diamond_ore", true, true,
        {11, 11, 11, 11, 11, 11}
    };

    for (int i = 12; i < 256; ++i) {
        m_entries[static_cast<BlockID>(i)] = {
            static_cast<BlockID>(i), "unknown", false, false,
            {-1, -1, -1, -1, -1, -1}
        };
    }
}

const BlockProperties& BlockRegistry::get(BlockID id) const {
    return m_entries[id];
}

bool BlockRegistry::isSolid(BlockID id) const {
    return m_entries[id].solid;
}

bool BlockRegistry::isOpaque(BlockID id) const {
    return m_entries[id].opaque;
}

int BlockRegistry::getFaceTexture(BlockID id, Face face) const {
    return m_entries[id].faceTexture[static_cast<std::uint8_t>(face)];
}

} // namespace voxel
