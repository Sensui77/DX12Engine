#include "TerrainGenerator.h"

#include <cfloat>

bool TerrainGenerator::Initialize(const TerrainParams& params) {
    m_params = params;
    return m_noiseGenerator.Initialize(params);
}

HeightfieldData TerrainGenerator::GenerateChunk(
    const GenerationContext& ctx,
    int cellsPerChunk,
    float cellSize,
    float worldX,
    float worldZ,
    float& outMinY,
    float& outMaxY) const {
    HeightfieldData data;
    const int vertexCountX = cellsPerChunk + 1;
    const int vertexCountZ = cellsPerChunk + 1;
    data.sampleCountX = vertexCountX + 2;
    data.sampleCountZ = vertexCountZ + 2;
    data.cellSize = cellSize;
    outMinY = FLT_MAX;
    outMaxY = -FLT_MAX;

    m_noiseGenerator.GenerateHeightmap(
        data.heights,
        data.sampleCountX,
        data.sampleCountZ,
        worldX - cellSize,
        worldZ - cellSize,
        cellSize,
        ctx);

    for (int z = 0; z < vertexCountZ; z++) {
        for (int x = 0; x < vertexCountX; x++) {
            const int sampleIndex = (z + 1) * data.sampleCountX + (x + 1);
            const float height = data.heights[sampleIndex];
            if (height < outMinY) {
                outMinY = height;
            }
            if (height > outMaxY) {
                outMaxY = height;
            }
        }
    }

    if (outMinY == FLT_MAX) {
        outMinY = 0.0f;
        outMaxY = 0.0f;
    }

    return data;
}
