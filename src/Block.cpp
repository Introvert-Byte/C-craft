#include "Block.h"

namespace voxel {

const BlockRegistry& blocks() {
    static const BlockRegistry registry;
    return registry;
}

BlockRegistry::BlockRegistry() {
    // Air.
    m_entries[BLOCK_AIR] = {BLOCK_AIR, "air", false, false, false};

    // Grass: solid, opaque. Top = grass, side = grass side, bottom = dirt.
    m_entries[BLOCK_GRASS] = {BLOCK_GRASS, "grass", true, true, false};

    // Dirt: solid, opaque.
    m_entries[BLOCK_DIRT] = {BLOCK_DIRT, "dirt", true, true, false};

    // Stone: solid, opaque.
    m_entries[BLOCK_STONE] = {BLOCK_STONE, "stone", true, true, false};

    // Sand: solid, opaque.
    m_entries[BLOCK_SAND] = {BLOCK_SAND, "sand", true, true, false};

    // Wood: solid, opaque.
    m_entries[BLOCK_WOOD] = {BLOCK_WOOD, "wood", true, true, false};

    // Leaves: solid but not opaque (translucent-ish), still culls faces.
    m_entries[BLOCK_LEAVES] = {BLOCK_LEAVES, "leaves", true, false, false};

    // Any unset ID defaults to air properties.
    for (int i = 7; i < 256; ++i) {
        m_entries[static_cast<BlockID>(i)] = {static_cast<BlockID>(i),
                                              "unknown", false, false, false};
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

} // namespace voxel
