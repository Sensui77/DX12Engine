#include "terrain/TerrainMeshBuilder.h"

#include <algorithm>
#include <cmath>

namespace {
struct HeightfieldBuildLayout {
    int sampleOffsetX = 0;
    int sampleOffsetZ = 0;
    int vertexCountX = 0;
    int vertexCountZ = 0;
};

bool ResolveBuildLayout(
    const HeightfieldData& heightfield,
    HeightfieldBorderMode borderMode,
    HeightfieldBuildLayout& outLayout) {
    if (!heightfield.IsValid()) {
        return false;
    }

    if (borderMode == HeightfieldBorderMode::PaddedOneSample) {
        if (heightfield.sampleCountX < 3 || heightfield.sampleCountZ < 3) {
            return false;
        }

        outLayout.sampleOffsetX = 1;
        outLayout.sampleOffsetZ = 1;
        outLayout.vertexCountX = heightfield.sampleCountX - 2;
        outLayout.vertexCountZ = heightfield.sampleCountZ - 2;
        return outLayout.vertexCountX > 0 && outLayout.vertexCountZ > 0;
    }

    outLayout.sampleOffsetX = 0;
    outLayout.sampleOffsetZ = 0;
    outLayout.vertexCountX = heightfield.sampleCountX;
    outLayout.vertexCountZ = heightfield.sampleCountZ;
    return outLayout.vertexCountX > 0 && outLayout.vertexCountZ > 0;
}

void BuildIndices(int cellsX, int cellsZ, std::vector<uint32_t>& outIndices) {
    const int vertexCountX = cellsX + 1;
    outIndices.clear();
    outIndices.reserve(static_cast<size_t>(cellsX) * static_cast<size_t>(cellsZ) * 6);

    for (int z = 0; z < cellsZ; z++) {
        for (int x = 0; x < cellsX; x++) {
            const uint32_t v00 = static_cast<uint32_t>(z * vertexCountX + x);
            const uint32_t v10 = v00 + 1;
            const uint32_t v01 = v00 + static_cast<uint32_t>(vertexCountX);
            const uint32_t v11 = v01 + 1;

            outIndices.push_back(v00);
            outIndices.push_back(v01);
            outIndices.push_back(v11);

            outIndices.push_back(v00);
            outIndices.push_back(v11);
            outIndices.push_back(v10);
        }
    }
}

MeshFloat4 BuildFlatTerrainColor() {
    return {0.18f, 0.55f, 0.18f, 1.0f};
}

std::array<float, 4> BuildTerrainColor(float height, const TerrainParams& params) {
    const float normalizedHeight = (params.heightScale > 0.1f) ? (height / params.heightScale) : 0.0f;
    return {
        0.12f + normalizedHeight * 0.5f,
        0.45f - normalizedHeight * 0.2f,
        0.08f + normalizedHeight * 0.2f,
        1.0f
    };
}

float SampleHeight(
    const HeightfieldData& heightfield,
    int sampleX,
    int sampleZ) {
    const int clampedX = std::clamp(sampleX, 0, heightfield.sampleCountX - 1);
    const int clampedZ = std::clamp(sampleZ, 0, heightfield.sampleCountZ - 1);
    return heightfield.heights[clampedZ * heightfield.sampleCountX + clampedX] * heightfield.verticalScale;
}

MeshFloat3 ComputeNormal(
    const HeightfieldData& heightfield,
    int sampleX,
    int sampleZ) {
    const float hLeft = SampleHeight(heightfield, sampleX - 1, sampleZ);
    const float hRight = SampleHeight(heightfield, sampleX + 1, sampleZ);
    const float hDown = SampleHeight(heightfield, sampleX, sampleZ - 1);
    const float hUp = SampleHeight(heightfield, sampleX, sampleZ + 1);

    const float invStep = (heightfield.cellSize > 0.0f) ? (0.5f / heightfield.cellSize) : 0.5f;
    const float dhdx = (hRight - hLeft) * invStep;
    const float dhdz = (hUp - hDown) * invStep;

    float nx = -dhdx;
    float ny = 1.0f;
    float nz = -dhdz;

    const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (length > 0.0001f) {
        nx /= length;
        ny /= length;
        nz /= length;
    } else {
        nx = 0.0f;
        ny = 1.0f;
        nz = 0.0f;
    }

    return {nx, ny, nz};
}

void ApplyFlatColors(MeshData& meshData) {
    for (MeshVertexData& vertex : meshData.vertices) {
        vertex.color = BuildFlatTerrainColor();
    }
}

void ApplyHeightColors(MeshData& meshData, const TerrainParams& params) {
    for (MeshVertexData& vertex : meshData.vertices) {
        vertex.color = BuildTerrainColor(vertex.position[1], params);
    }
}
} // namespace

MeshDataStreams TerrainMeshBuilder::BuildFlatMeshStreams(
    int cellsX,
    int cellsZ,
    float cellSize,
    float originX,
    float originZ,
    bool generateNormals) {
    MeshDataStreams meshData;

    const int vertexCountX = cellsX + 1;
    const int vertexCountZ = cellsZ + 1;
    const size_t vertexCount = static_cast<size_t>(vertexCountX) * static_cast<size_t>(vertexCountZ);
    meshData.positions.reserve(vertexCount);
    if (generateNormals) {
        meshData.normals.reserve(vertexCount);
    }

    for (int z = 0; z < vertexCountZ; z++) {
        for (int x = 0; x < vertexCountX; x++) {
            meshData.positions.push_back({
                originX + static_cast<float>(x) * cellSize,
                0.0f,
                originZ + static_cast<float>(z) * cellSize
            });
            if (generateNormals) {
                meshData.normals.push_back({0.0f, 1.0f, 0.0f});
            }
        }
    }

    BuildIndices(cellsX, cellsZ, meshData.indices);
    return meshData;
}

MeshDataStreams TerrainMeshBuilder::BuildHeightfieldMeshStreams(
    const HeightfieldData& heightfield,
    const HeightfieldMeshBuildOptions& options) {
    MeshDataStreams meshData;
    HeightfieldBuildLayout layout;

    if (!ResolveBuildLayout(heightfield, options.borderMode, layout)) {
        return meshData;
    }

    const size_t vertexCount = static_cast<size_t>(layout.vertexCountX) * static_cast<size_t>(layout.vertexCountZ);
    meshData.positions.reserve(vertexCount);
    if (options.generateNormals) {
        meshData.normals.reserve(vertexCount);
    }

    for (int z = 0; z < layout.vertexCountZ; z++) {
        for (int x = 0; x < layout.vertexCountX; x++) {
            const int sampleX = x + layout.sampleOffsetX;
            const int sampleZ = z + layout.sampleOffsetZ;
            const float height = SampleHeight(heightfield, sampleX, sampleZ);

            meshData.positions.push_back({
                options.originX + static_cast<float>(x) * heightfield.cellSize,
                height,
                options.originZ + static_cast<float>(z) * heightfield.cellSize
            });

            if (options.generateNormals) {
                meshData.normals.push_back(ComputeNormal(heightfield, sampleX, sampleZ));
            }
        }
    }

    BuildIndices(layout.vertexCountX - 1, layout.vertexCountZ - 1, meshData.indices);
    return meshData;
}

MeshData TerrainMeshBuilder::BuildFlatMesh(
    int cellsX,
    int cellsZ,
    float cellSize,
    float originX,
    float originZ) {
    MeshData meshData = ConvertMeshDataToAoS(BuildFlatMeshStreams(
        cellsX,
        cellsZ,
        cellSize,
        originX,
        originZ));
    ApplyFlatColors(meshData);
    return meshData;
}

MeshData TerrainMeshBuilder::BuildHeightfieldMesh(
    const HeightfieldData& heightfield,
    const TerrainParams& params,
    float originX,
    float originZ) {
    const HeightfieldMeshBuildOptions options{
        originX,
        originZ,
        HeightfieldBorderMode::PaddedOneSample,
        true
    };
    MeshData meshData = ConvertMeshDataToAoS(BuildHeightfieldMeshStreams(heightfield, options));
    ApplyHeightColors(meshData, params);
    return meshData;
}
