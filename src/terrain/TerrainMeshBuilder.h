#pragma once

#include "HeightfieldData.h"
#include "MeshData.h"
#include "games/interactive_terrain/TerrainTypes.h"

enum class HeightfieldBorderMode {
    PaddedOneSample,
    NoBorder
};

struct HeightfieldMeshBuildOptions {
    float originX = 0.0f;
    float originZ = 0.0f;
    HeightfieldBorderMode borderMode = HeightfieldBorderMode::PaddedOneSample;
    bool generateNormals = true;
};

class TerrainMeshBuilder {
public:
    static MeshDataStreams BuildFlatMeshStreams(
        int cellsX,
        int cellsZ,
        float cellSize,
        float originX,
        float originZ,
        bool generateNormals = true);

    static MeshDataStreams BuildHeightfieldMeshStreams(
        const HeightfieldData& heightfield,
        const HeightfieldMeshBuildOptions& options = {});

    static MeshData BuildFlatMesh(
        int cellsX,
        int cellsZ,
        float cellSize,
        float originX,
        float originZ);

    // Expects a regular heightfield with a 1-sample border around the renderable interior.
    // The builder uses that padding to compute normals without reaching outside the array.
    static MeshData BuildHeightfieldMesh(
        const HeightfieldData& heightfield,
        const TerrainParams& params,
        float originX,
        float originZ);
};
