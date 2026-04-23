#pragma once

#include <memory>
#include <vector>

#include "Mesh.h"
#include "MeshData.h"

class CopyEngine;

class MeshRenderProxy {
public:
    bool Initialize(
        ID3D12Device* device,
        ID3D12CommandQueue* directQueue,
        CopyEngine& copyCtx,
        const std::vector<MeshDataStreams>& meshes);
    bool Initialize(
        ID3D12Device* device,
        ID3D12CommandQueue* directQueue,
        CopyEngine& copyCtx,
        const std::vector<MeshData>& meshes);
    void Clear();

    void StageMesh(size_t meshIndex, const MeshDataStreams& meshData, uint32_t frameIndex);
    void StageMesh(size_t meshIndex, const MeshData& meshData, uint32_t frameIndex);
    void RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex);
    void DrawAll(ID3D12GraphicsCommandList* cmdList) const;
    void DrawOne(ID3D12GraphicsCommandList* cmdList, size_t meshIndex) const;

    size_t GetMeshCount() const { return m_meshes.size(); }

private:
    std::vector<std::unique_ptr<Mesh>> m_meshes;
};
