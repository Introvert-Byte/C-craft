#include "Block.h"

namespace voxel {

const BlockRegistry& blocks() {
    static const BlockRegistry registry;
    return registry;
}

BlockRegistry::BlockRegistry() {
    m_entries[BLOCK_AIR] = {BLOCK_AIR, "air", false, false};

    m_entries[BLOCK_GRASS] = {BLOCK_GRASS, "grass", true, true};
    m_entries[BLOCK_DIRT]  = {BLOCK_DIRT,  "dirt",  true, true};
    m_entries[BLOCK_STONE] = {BLOCK_STONE, "stone", true, true};
    m_entries[BLOCK_SAND]  = {BLOCK_SAND,  "sand",  true, true};
    m_entries[BLOCK_WOOD]  = {BLOCK_WOOD,  "wood",  true, true};
    m_entries[BLOCK_LEAVES]= {BLOCK_LEAVES,"leaves",true, false};

    for (int i = 7; i < 256; ++i) {
        m_entries[static_cast<BlockID>(i)] = {static_cast<BlockID>(i), "unknown", false, false};
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
