#pragma once
#include <cstdint>

//noise noise
uint32_t hashNoise(uint32_t x, uint32_t seed)
{
    uint32_t h = x ^ seed;
    h ^= h >> 16;
    h *= 0x7feb352d;
    h ^= h >> 15;
    h *= 0x846ca68b;
    h ^= h >> 16;
    return h;
}

float noise1D(int x, uint32_t seed)
{
    uint32_t h = hashNoise(static_cast<uint32_t>(x), seed);

    //0-1
    return static_cast<float>(h) /
        static_cast<float>(UINT32_MAX);
}

float noise2D(int x, int y, uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(x);
    h ^= static_cast<uint32_t>(y) * 0x9e3779b9u;

    h = hashNoise(h, seed);

    //0-1
    return static_cast<float>(h) /
        static_cast<float>(UINT32_MAX);
}