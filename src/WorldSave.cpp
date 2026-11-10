#include "WorldSave.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

namespace voxel {

WorldSave::WorldSave(std::filesystem::path savesDir)
    : m_savesDir(std::move(savesDir)) {
    std::error_code ec;
    std::filesystem::create_directories(m_savesDir, ec);
}

std::filesystem::path WorldSave::worldDir(const std::string& name) const {
    return m_savesDir / name;
}

std::filesystem::path WorldSave::chunkFile(const std::string& name, int cx,
                                           int cz) const {
    return worldDir(name) / ("c_" + std::to_string(cx) + "_" +
                             std::to_string(cz) + ".dat");
}

std::vector<WorldSaveInfo> WorldSave::listWorlds() const {
    std::vector<WorldSaveInfo> result;
    if (!std::filesystem::exists(m_savesDir)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(m_savesDir)) {
        if (!entry.is_directory()) continue;
        WorldSaveInfo info;
        info.name = entry.path().filename().string();
        info.path = entry.path();
        info.exists = true;
        try {
            info.lastModified =
                std::filesystem::last_write_time(entry.path())
                    .time_since_epoch()
                    .count();
        } catch (...) {
            info.lastModified = 0;
        }
        result.push_back(info);
    }
    std::sort(result.begin(), result.end(),
              [](const WorldSaveInfo& a, const WorldSaveInfo& b) {
                  return a.name < b.name;
              });
    return result;
}

bool WorldSave::worldExists(const std::string& name) const {
    return std::filesystem::exists(worldDir(name));
}

bool WorldSave::createWorld(const std::string& name) {
    if (name.empty() || worldExists(name)) return false;
    std::error_code ec;
    return std::filesystem::create_directories(worldDir(name), ec);
}

bool WorldSave::writeChunk(const std::filesystem::path& path,
                           const Chunk& chunk) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    constexpr char MAGIC[4] = {'C', 'C', 'K', '0'};
    file.write(MAGIC, 4);
    std::int32_t cx = chunk.chunkX();
    std::int32_t cz = chunk.chunkZ();
    file.write(reinterpret_cast<const char*>(&cx), sizeof(cx));
    file.write(reinterpret_cast<const char*>(&cz), sizeof(cz));

    std::uint8_t buf[Chunk::SIZE];
    for (int i = 0; i < Chunk::SIZE; ++i) {
        int x = i % Chunk::WIDTH;
        int y = (i / Chunk::WIDTH) % Chunk::HEIGHT;
        int z = i / (Chunk::WIDTH * Chunk::HEIGHT);
        buf[i] = static_cast<std::uint8_t>(chunk.getBlock(x, y, z));
    }
    file.write(reinterpret_cast<const char*>(buf), Chunk::SIZE);
    return file.good();
}

bool WorldSave::readChunk(const std::filesystem::path& path, Chunk& chunk) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "CCK0", 4) != 0) return false;

    std::int32_t cx = 0, cz = 0;
    file.read(reinterpret_cast<char*>(&cx), sizeof(cx));
    file.read(reinterpret_cast<char*>(&cz), sizeof(cz));

    std::uint8_t buf[Chunk::SIZE];
    file.read(reinterpret_cast<char*>(buf), Chunk::SIZE);
    if (!file.good()) return false;

    for (int i = 0; i < Chunk::SIZE; ++i) {
        int x = i % Chunk::WIDTH;
        int y = (i / Chunk::WIDTH) % Chunk::HEIGHT;
        int z = i / (Chunk::WIDTH * Chunk::HEIGHT);
        chunk.setBlock(x, y, z, static_cast<BlockID>(buf[i]));
    }
    chunk.setMeshDirty(true);
    return true;
}

bool WorldSave::saveWorld(const std::string& name, const World& world) {
    if (!createWorld(name)) return false;
    bool ok = true;
    world.forEachChunk([&](int cx, int cz, Chunk* chunk) {
        if (!writeChunk(chunkFile(name, cx, cz), *chunk)) ok = false;
    });
    return ok;
}

bool WorldSave::loadWorld(const std::string& name, World& world) {
    if (!worldExists(name)) return false;

    auto dir = worldDir(name);
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".dat") continue;
        std::string fname = entry.path().filename().string();
        if (fname.size() < 3 || fname[0] != 'c' || fname[1] != '_')
            continue;
        size_t first = fname.find('_', 2);
        size_t second = fname.find('_', first + 1);
        if (first == std::string::npos || second == std::string::npos)
            continue;
        int cx = std::stoi(fname.substr(first + 1, second - first - 1));
        int cz = std::stoi(fname.substr(second + 1,
                                         fname.size() - second -
                                             std::string(".dat").size()));
        world.generateChunkAsync(cx, cz);
        Chunk* c = world.getChunk(cx, cz);
        if (!c) continue;
        readChunk(entry.path(), *c);
        c->setMeshDirty(true);
    }
    return true;
}

bool WorldSave::deleteWorld(const std::string& name) {
    if (!worldExists(name)) return false;
    std::error_code ec;
    return std::filesystem::remove_all(worldDir(name), ec) > 0;
}

} // namespace voxel
