#pragma once
#include <FastNoise/FastNoise.h>
#include <vector>
#include <cstdint>
#include "TerrainTypes.h"

class NoiseGenerator {
public:
    NoiseGenerator() = default;
    ~NoiseGenerator() = default;

    bool Initialize(const TerrainParams& params = TerrainParams());

    // Generate a heightmap for the given chunk.
    // Output vector is resized to width*height.
    // Values are in world-space Y (already multiplied by heightScale).
    void GenerateHeightmap(
        std::vector<float>& outHeights,
        int width, int height,
        float startX, float startZ,
        float stepSize,
        const GenerationContext& ctx) const;

    const TerrainParams& GetParams() const { return m_params; }

private:
    TerrainParams m_params;
    FastNoise::SmartNode<> m_generator;  // Root of the node graph
};
