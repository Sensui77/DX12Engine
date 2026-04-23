#pragma once

#include <cfloat>
#include <cstdint>
#include <memory>
#include <vector>

#include "MeshData.h"
#include "TerrainTypes.h"

struct TerrainChunkData {
    float worldX = 0.0f;
    float worldZ = 0.0f;
    size_t index = 0;
    bool needsUpdate = false;
    uint64_t queuedRevision = 0;
    uint64_t appliedRevision = 0;

    // Height bounds for frustum culling (updated after noise gen)
    float minY = 0.0f;
    float maxY = 0.0f;
};

struct TerrainWorldSettings {
    int chunksX = 6;
    int chunksZ = 3;
    int cellsPerChunk = 128;
    float chunkWorldSize = 128.0f;
    float cellSize = 1.0f;
    int pendingResolution = 128;
};

void InitializeTerrainWorld(
    std::vector<std::unique_ptr<TerrainChunkData>>& world,
    TerrainWorldSettings& settings);

std::vector<MeshDataStreams> BuildInitialTerrainMeshes(
    const std::vector<std::unique_ptr<TerrainChunkData>>& world,
    const TerrainWorldSettings& settings);
