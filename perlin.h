#pragma once
#include <random>

//I'm too dumb for this shi
//Of course i copied it off of the internet
namespace PERLIN {
    uint8_t PERM[512];

    void generatePermutation(unsigned seed)
    {
        uint8_t p[256];

        for (int i = 0; i < 256; ++i)
            p[i] = static_cast<uint8_t>(i);

        std::mt19937 rng(seed);
        std::shuffle(p, p + 256, rng);

        for (int i = 0; i < 512; ++i)
            PERM[i] = p[i & 255];
    }


    static constexpr float INV_SQRT2 = 0.7071067811865475f;

    static constexpr float GRAD_X[8] = {
         1.0f, -1.0f,  1.0f, -1.0f,
         INV_SQRT2, -INV_SQRT2, INV_SQRT2, -INV_SQRT2
    };

    static constexpr float GRAD_Y[8] = {
         1.0f,  1.0f, -1.0f, -1.0f,
         INV_SQRT2, INV_SQRT2, -INV_SQRT2, -INV_SQRT2
    };


    static inline uint32_t hash2(int x, int y)
    {
        return PERM[(PERM[x & 255] + (y & 255)) & 255];
    }


    static inline float gradDot(
        int hash,
        float dx,
        float dy)
    {
        const int g = hash & 7;

        return GRAD_X[g] * dx +
            GRAD_Y[g] * dy;
    }


    static inline float fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }


    static inline float perlin(float x, float y)
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));

        const float tx = x - x0;
        const float ty = y - y0;

        const float u = fade(tx);
        const float v = fade(ty);

        const int X = x0 & 255;
        const int Y = y0 & 255;

        const int A = PERM[X] + Y;
        const int B = PERM[X + 1] + Y;

        const float n00 = gradDot(PERM[A], tx, ty);
        const float n10 = gradDot(PERM[B], tx - 1, ty);
        const float n01 = gradDot(PERM[A + 1], tx, ty - 1);
        const float n11 = gradDot(PERM[B + 1], tx - 1, ty - 1);

        const float nx0 = n00 + u * (n10 - n00);
        const float nx1 = n01 + u * (n11 - n01);

        return nx0 + v * (nx1 - nx0);
    }


    static inline float fbm(
        float x,
        float y,
        int octaves)
    {
        float value = 0.0f;

        float amplitude = 1.0f;
        float frequency = 1.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += perlin(
                x * frequency,
                y * frequency
            ) * amplitude;

            frequency *= 2.0f;
            amplitude *= 0.5f;
        }

        return value;
    }



}