#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

using MeshFloat2 = std::array<float, 2>;
using MeshFloat3 = std::array<float, 3>;
using MeshFloat4 = std::array<float, 4>;

struct MeshVertexData {
    MeshFloat3 position{0.0f, 0.0f, 0.0f};
    MeshFloat4 color{1.0f, 1.0f, 1.0f, 1.0f};
    MeshFloat3 normal{0.0f, 1.0f, 0.0f};
};

struct MeshData {
    std::vector<MeshVertexData> vertices;
    std::vector<uint32_t> indices;
};

// Stream-oriented CPU mesh representation for staged procedural generation.
// Positions are required. Other attribute streams are optional, but if present
// they must match the position count.
struct MeshDataStreams {
    std::vector<MeshFloat3> positions;
    std::vector<MeshFloat3> normals;
    std::vector<MeshFloat4> colors;
    std::vector<MeshFloat2> uvs;
    std::vector<uint32_t> indices;

    size_t VertexCount() const { return positions.size(); }
    bool HasNormals() const { return !normals.empty(); }
    bool HasColors() const { return !colors.empty(); }
    bool HasUvs() const { return !uvs.empty(); }

    bool IsValid() const {
        const size_t vertexCount = positions.size();
        return (!HasNormals() || normals.size() == vertexCount)
            && (!HasColors() || colors.size() == vertexCount)
            && (!HasUvs() || uvs.size() == vertexCount);
    }
};

MeshDataStreams ConvertMeshDataToStreams(const MeshData& meshData);
MeshData ConvertMeshDataToAoS(const MeshDataStreams& meshStreams);
