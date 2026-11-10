#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Chunk.h"
#include "World.h"

namespace voxel {

struct WorldSaveInfo {
    std::string name;
    std::filesystem::path path;
    std::uint64_t lastModified = 0;
    bool exists = false;
};

class WorldSave {
public:
    explicit WorldSave(std::filesystem::path savesDir);

    std::vector<WorldSaveInfo> listWorlds() const;
    bool worldExists(const std::string& name) const;
    bool createWorld(const std::string& name);
    bool saveWorld(const std::string& name, const World& world);
    bool loadWorld(const std::string& name, World& world);
    bool deleteWorld(const std::string& name);

    const std::filesystem::path& savesDir() const { return m_savesDir; }

private:
    std::filesystem::path m_savesDir;

    std::filesystem::path worldDir(const std::string& name) const;
    std::filesystem::path chunkFile(const std::string& name, int cx, int cz) const;
    bool writeChunk(const std::filesystem::path& path, const Chunk& chunk);
    bool readChunk(const std::filesystem::path& path, Chunk& chunk);
};

} // namespace voxel
