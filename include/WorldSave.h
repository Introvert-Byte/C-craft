#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Chunk.h"
#include "World.h"

namespace voxel {

// Information about a saved world on disk.
struct WorldSaveInfo {
    std::string name;
    std::filesystem::path path;
    std::uint64_t lastModified = 0;
    bool exists = false;
};

// World save/load manager. Persists chunk data to disk so worlds persist
// between sessions.
class WorldSave {
public:
    explicit WorldSave(std::filesystem::path savesDir);

    // List all saved worlds in the saves directory.
    std::vector<WorldSaveInfo> listWorlds() const;

    // Check if a world with the given name exists.
    bool worldExists(const std::string& name) const;

    // Create a new (empty) world directory. Returns true on success.
    bool createWorld(const std::string& name);

    // Save all loaded chunks to disk for the given world.
    bool saveWorld(const std::string& name, const World& world);

    // Load chunks from disk into the world for the given world.
    bool loadWorld(const std::string& name, World& world);

    // Delete a world and all its data.
    bool deleteWorld(const std::string& name);

    const std::filesystem::path& savesDir() const { return m_savesDir; }

private:
    std::filesystem::path m_savesDir;

    std::filesystem::path worldDir(const std::string& name) const;
    std::filesystem::path chunkFile(const std::string& name, int cx,
                                    int cz) const;
    bool writeChunk(const std::filesystem::path& path, const Chunk& chunk);
    bool readChunk(const std::filesystem::path& path, Chunk& chunk);
};

} // namespace voxel
