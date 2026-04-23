#pragma once

#include "NoiseGenerator.h"
#include "TerrainTypes.h"
#include "terrain/HeightfieldData.h"

class TerrainGenerator {
public:
    TerrainGenerator() = default;
    ~TerrainGenerator() = default;

    bool Initialize(const TerrainParams& params = TerrainParams());

    HeightfieldData GenerateChunk(
        const GenerationContext& ctx,
        int cellsPerChunk,
        float cellSize,
        float worldX,
        float worldZ,
        float& outMinY,
        float& outMaxY) const;

private:
    TerrainParams m_params;
    NoiseGenerator m_noiseGenerator;
};
