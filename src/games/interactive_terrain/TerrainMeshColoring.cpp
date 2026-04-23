#include "TerrainMeshColoring.h"

namespace {
constexpr MeshFloat4 kFlatTerrainColor{0.18f, 0.55f, 0.18f, 1.0f};

MeshFloat4 BuildHeightColor(float height, const TerrainParams& params) {
    const float normalizedHeight = (params.heightScale > 0.1f) ? (height / params.heightScale) : 0.0f;
    return {
        0.12f + normalizedHeight * 0.5f,
        0.45f - normalizedHeight * 0.2f,
        0.08f + normalizedHeight * 0.2f,
        1.0f
    };
}
} // namespace

namespace InteractiveTerrainMeshColoring {

void ApplyFlatTerrainColors(MeshDataStreams& meshData) {
    meshData.colors.assign(meshData.positions.size(), kFlatTerrainColor);
}

void ApplyHeightTerrainColors(MeshDataStreams& meshData, const TerrainParams& params) {
    meshData.colors.clear();
    meshData.colors.reserve(meshData.positions.size());

    for (const MeshFloat3& position : meshData.positions) {
        meshData.colors.push_back(BuildHeightColor(position[1], params));
    }
}

} // namespace InteractiveTerrainMeshColoring
