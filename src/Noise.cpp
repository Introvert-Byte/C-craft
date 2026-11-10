#include "Noise.h"

#include <algorithm>
#include <cmath>

namespace voxel {

PerlinNoise::PerlinNoise(std::int32_t seed) {
    std::array<std::int32_t, 256> p{};
    for (std::int32_t i = 0; i < 256; ++i) p[i] = i;

    // Deterministic shuffle seeded by `seed`.
    std::int32_t s = static_cast<std::int32_t>(seed);
    for (std::int32_t i = 255; i > 0; --i) {
        s = s * 1103515245 + 12345;
        std::int32_t j = (s >> 16) % (i + 1);
        std::swap(p[i], p[j]);
    }

    // Duplicate to avoid modulo in the lookup.
    for (std::int32_t i = 0; i < 512; ++i) m_p[i] = p[i % 256];
}

double PerlinNoise::fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double PerlinNoise::lerp(double a, double b, double t) {
    return a + t * (b - a);
}

double PerlinNoise::grad(std::int32_t hash, double x, double y, double z) {
    std::int32_t h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    double r = ((h & 1) == 0 ? -u : u) + ((h & 2) == 0 ? -v : v);
    return r;
}

double PerlinNoise::noise2D(double x, double y) const {
    std::int32_t X = static_cast<std::int32_t>(std::floor(x)) & 255;
    std::int32_t Y = static_cast<std::int32_t>(std::floor(y)) & 255;

    double xf = x - std::floor(x);
    double yf = y - std::floor(y);
    double u = fade(xf);
    double v = fade(yf);

    std::int32_t a = m_p[X] + Y;
    std::int32_t b = m_p[X + 1] + Y;

    double g00 = grad(m_p[a], xf, yf, 0.0);
    double g10 = grad(m_p[b], xf - 1.0, yf, 0.0);
    double g01 = grad(m_p[a + 1], xf, yf - 1.0, 0.0);
    double g11 = grad(m_p[b + 1], xf - 1.0, yf - 1.0, 0.0);

    return lerp(lerp(g00, g10, u), lerp(g01, g11, u), v);
}

double PerlinNoise::noise3D(double x, double y, double z) const {
    std::int32_t X = static_cast<std::int32_t>(std::floor(x)) & 255;
    std::int32_t Y = static_cast<std::int32_t>(std::floor(y)) & 255;
    std::int32_t Z = static_cast<std::int32_t>(std::floor(z)) & 255;

    double xf = x - std::floor(x);
    double yf = y - std::floor(y);
    double zf = z - std::floor(z);
    double u = fade(xf);
    double v = fade(yf);
    double w = fade(zf);

    std::int32_t a = m_p[X] + Y;
    std::int32_t b = m_p[X + 1] + Y;
    std::int32_t aa = m_p[a] + Z;
    std::int32_t ab = m_p[a + 1] + Z;
    std::int32_t ba = m_p[b] + Z;
    std::int32_t bb = m_p[b + 1] + Z;

    double g000 = grad(m_p[aa], xf, yf, zf);
    double g100 = grad(m_p[ba], xf - 1.0, yf, zf);
    double g010 = grad(m_p[ab], xf, yf - 1.0, zf);
    double g110 = grad(m_p[bb], xf - 1.0, yf - 1.0, zf);
    double g001 = grad(m_p[aa + 1], xf, yf, zf - 1.0);
    double g101 = grad(m_p[ba + 1], xf - 1.0, yf, zf - 1.0);
    double g011 = grad(m_p[ab + 1], xf, yf - 1.0, zf - 1.0);
    double g111 = grad(m_p[bb + 1], xf - 1.0, yf - 1.0, zf - 1.0);

    return lerp(lerp(lerp(g000, g100, u),
                     lerp(g010, g110, u), v),
                lerp(lerp(g001, g101, u),
                     lerp(g011, g111, u), v), w);
}

double PerlinNoise::fbm2D(double x, double y, int octaves, double lacunarity,
                          double gain) const {
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double maxValue = 0.0;
    for (int i = 0; i < octaves; ++i) {
        total += noise2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return total / maxValue;
}

double PerlinNoise::fbm3D(double x, double y, double z, int octaves,
                          double lacunarity, double gain) const {
    double total = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double maxValue = 0.0;
    for (int i = 0; i < octaves; ++i) {
        total += noise3D(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return total / maxValue;
}

} // namespace voxel
