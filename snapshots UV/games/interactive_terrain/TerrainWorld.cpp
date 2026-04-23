#include "TerrainWorld.h"

#include "TerrainMeshColoring.h"
#include "terrain/TerrainMeshBuilder.h"

void InitializeTerrainWorld(
    std::vector<std::unique_ptr<TerrainChunkData>>& world,
    TerrainWorldSettings& settings)
{
    world.clear();
    world.reserve(static_cast<size_t>(settings.chunksX) * static_cast<size_t>(settings.chunksZ));

    settings.cellSize = settings.chunkWorldSize / static_cast<float>(settings.cellsPerChunk);

    float totalW = settings.chunksX * settings.chunkWorldSize;
    float totalH = settings.chunksZ * settings.chunkWorldSize;
    float startX = -totalW * 0.5f;
    float startZ = -totalH * 0.5f;

    size_t chunkIndex = 0;
    for (int z = 0; z < settings.chunksZ; z++) {
        for (int x = 0; x < settings.chunksX; x++) {
            auto chunk = std::make_unique<TerrainChunkData>();
            chunk->index = chunkIndex++;
            chunk->worldX = startX + x * settings.chunkWorldSize;
            chunk->worldZ = startZ + z * settings.chunkWorldSize;
            world.push_back(std::move(chunk));
        }
    }
}

std::vector<MeshDataStreams> BuildInitialTerrainMeshes(
    const std::vector<std::unique_ptr<TerrainChunkData>>& world,
    const TerrainWorldSettings& settings)
{
    std::vector<MeshDataStreams> meshes;
    meshes.reserve(world.size());

    for (const auto& chunk : world) {
        MeshDataStreams meshStreams = TerrainMeshBuilder::BuildFlatMeshStreams(
            settings.cellsPerChunk,
            settings.cellsPerChunk,
            settings.cellSize,
            chunk->worldX,
            chunk->worldZ);
        InteractiveTerrainMeshColoring::ApplyFlatTerrainColors(meshStreams);
        meshes.push_back(std::move(meshStreams));
    }

    return meshes;
}
