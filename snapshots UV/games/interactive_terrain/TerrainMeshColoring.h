#pragma once

#include "MeshData.h"
#include "TerrainTypes.h"

namespace InteractiveTerrainMeshColoring {

void ApplyFlatTerrainColors(MeshDataStreams& meshData);
void ApplyHeightTerrainColors(MeshDataStreams& meshData, const TerrainParams& params);

}
