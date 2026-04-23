#include "rendering/MeshData.h"

namespace {
constexpr MeshFloat4 kDefaultVertexColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr MeshFloat3 kDefaultVertexNormal{0.0f, 1.0f, 0.0f};
} // namespace

MeshDataStreams ConvertMeshDataToStreams(const MeshData& meshData) {
    MeshDataStreams meshStreams;
    meshStreams.positions.reserve(meshData.vertices.size());
    meshStreams.normals.reserve(meshData.vertices.size());
    meshStreams.colors.reserve(meshData.vertices.size());
    meshStreams.indices = meshData.indices;

    for (const MeshVertexData& vertex : meshData.vertices) {
        meshStreams.positions.push_back(vertex.position);
        meshStreams.normals.push_back(vertex.normal);
        meshStreams.colors.push_back(vertex.color);
    }

    return meshStreams;
}

MeshData ConvertMeshDataToAoS(const MeshDataStreams& meshStreams) {
    MeshData meshData;
    if (!meshStreams.IsValid()) {
        return meshData;
    }

    meshData.vertices.reserve(meshStreams.positions.size());
    meshData.indices = meshStreams.indices;

    for (size_t i = 0; i < meshStreams.positions.size(); ++i) {
        MeshVertexData vertex;
        vertex.position = meshStreams.positions[i];
        vertex.normal = meshStreams.HasNormals() ? meshStreams.normals[i] : kDefaultVertexNormal;
        vertex.color = meshStreams.HasColors() ? meshStreams.colors[i] : kDefaultVertexColor;
        meshData.vertices.push_back(vertex);
    }

    return meshData;
}
