#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace voxel {

class PerlinNoise {
public:
    explicit PerlinNoise(std::int32_t seed = 1337);

    double noise2D(double x, double y) const;
    double noise3D(double x, double y, double z) const;

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
