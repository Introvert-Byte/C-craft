#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace voxel {

// Classic Perlin noise (improved). Self-contained, no external dependency.
// Provides coherent 2D and 3D noise useful for terrain heightmaps and caves.
class PerlinNoise {
public:
    explicit PerlinNoise(std::int32_t seed = 1337);

    // Noise value in [-1, 1].
    double noise2D(double x, double y) const;
    double noise3D(double x, double y, double z) const;

    // Fractal Brownian Motion (fBm): layered noise for detail.
    double fbm2D(double x, double y, int octaves = 4,
                 double lacunarity = 2.0, double gain = 0.5) const;
    double fbm3D(double x, double y, double z, int octaves = 4,
                 double lacunarity = 2.0, double gain = 0.5) const;

private:
    static double fade(double t);
    static double lerp(double a, double b, double t);
    static double grad(std::int32_t hash, double x, double y, double z);

    std::array<std::int32_t, 512> m_p;
};

} // namespace voxel
