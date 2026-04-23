#include "rendering/MeshRenderProxy.h"

#include <iostream>
#include <vector>
#include <wrl/client.h>

#include "CopyEngine.h"
#include "rendering/RenderVertex.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr MeshFloat4 kDefaultVertexColor{1.0f, 1.0f, 1.0f, 1.0f};
constexpr MeshFloat3 kDefaultVertexNormal{0.0f, 1.0f, 0.0f};

std::vector<RenderVertex> BuildRenderVertices(const MeshDataStreams& meshData) {
    std::vector<RenderVertex> renderVertices;
    if (!meshData.IsValid()) {
        return renderVertices;
    }

    renderVertices.reserve(meshData.positions.size());
    for (size_t i = 0; i < meshData.positions.size(); ++i) {
        RenderVertex vertex = {};
        const MeshFloat3& position = meshData.positions[i];
        const MeshFloat4& color = meshData.HasColors() ? meshData.colors[i] : kDefaultVertexColor;
        const MeshFloat3& normal = meshData.HasNormals() ? meshData.normals[i] : kDefaultVertexNormal;

        vertex.position[0] = position[0];
        vertex.position[1] = position[1];
        vertex.position[2] = position[2];
        vertex.color[0] = color[0];
        vertex.color[1] = color[1];
        vertex.color[2] = color[2];
        vertex.color[3] = color[3];
        vertex.normal[0] = normal[0];
        vertex.normal[1] = normal[1];
        vertex.normal[2] = normal[2];
        renderVertices.push_back(vertex);
    }

    return renderVertices;
}
} // namespace

bool MeshRenderProxy::Initialize(
    ID3D12Device* device,
    ID3D12CommandQueue* directQueue,
    CopyEngine& copyCtx,
    const std::vector<MeshDataStreams>& meshes)
{
    m_meshes.clear();
    m_meshes.reserve(meshes.size());

    for (const MeshDataStreams& meshData : meshes) {
        const std::vector<RenderVertex> renderVertices = BuildRenderVertices(meshData);
        auto mesh = std::make_unique<Mesh>();
        if (!mesh->Initialize(
                device,
                sizeof(RenderVertex),
                renderVertices.data(),
                static_cast<uint32_t>(renderVertices.size()),
                meshData.indices.data(),
                static_cast<uint32_t>(meshData.indices.size()))) {
            m_meshes.clear();
            return false;
        }
        m_meshes.push_back(std::move(mesh));
    }

    auto* copyCmdList = copyCtx.Begin();
    if (!copyCmdList) {
        m_meshes.clear();
        return false;
    }

    for (const auto& mesh : m_meshes) {
        mesh->RecordInitialCopy(copyCmdList);
    }

    if (!copyCtx.ExecuteAndWait()) {
        m_meshes.clear();
        return false;
    }

    {
        ComPtr<ID3D12CommandAllocator> allocator;
        if (FAILED(device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) {
            m_meshes.clear();
            return false;
        }

        ComPtr<ID3D12GraphicsCommandList> cmdList;
        if (FAILED(device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList)))) {
            m_meshes.clear();
            return false;
        }

        for (const auto& mesh : m_meshes) {
            mesh->RecordTransitionToRenderState(cmdList.Get());
        }

        cmdList->Close();
        ID3D12CommandList* lists[] = { cmdList.Get() };
        directQueue->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> fence;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
            m_meshes.clear();
            return false;
        }

        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!event) {
            m_meshes.clear();
            return false;
        }

        directQueue->Signal(fence.Get(), 1);
        if (fence->GetCompletedValue() < 1) {
            fence->SetEventOnCompletion(1, event);
            WaitForSingleObject(event, INFINITE);
        }
        CloseHandle(event);
    }

    for (const auto& mesh : m_meshes) {
        mesh->FinalizeInitialUpload();
    }

    std::cout << "[MeshRenderProxy] Batch upload: " << m_meshes.size()
              << " meshes (COPY queue + DIRECT transitions)\n";

    return true;
}

bool MeshRenderProxy::Initialize(
    ID3D12Device* device,
    ID3D12CommandQueue* directQueue,
    CopyEngine& copyCtx,
    const std::vector<MeshData>& meshes)
{
    std::vector<MeshDataStreams> streamMeshes;
    streamMeshes.reserve(meshes.size());
    for (const MeshData& meshData : meshes) {
        streamMeshes.push_back(ConvertMeshDataToStreams(meshData));
    }
    return Initialize(device, directQueue, copyCtx, streamMeshes);
}

void MeshRenderProxy::Clear() {
    m_meshes.clear();
}

void MeshRenderProxy::StageMesh(size_t meshIndex, const MeshDataStreams& meshData, uint32_t frameIndex) {
    if (meshIndex >= m_meshes.size() || !m_meshes[meshIndex]) {
        return;
    }

    const std::vector<RenderVertex> renderVertices = BuildRenderVertices(meshData);
    m_meshes[meshIndex]->StageVertices(
        renderVertices.data(),
        static_cast<uint32_t>(renderVertices.size() * sizeof(RenderVertex)),
        frameIndex);
}

void MeshRenderProxy::StageMesh(size_t meshIndex, const MeshData& meshData, uint32_t frameIndex) {
    StageMesh(meshIndex, ConvertMeshDataToStreams(meshData), frameIndex);
}

void MeshRenderProxy::RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    std::vector<size_t> dirtyIndices;

    for (size_t i = 0; i < m_meshes.size(); i++) {
        if (m_meshes[i] && m_meshes[i]->NeedsUpload(frameIndex)) {
            barriers.push_back(m_meshes[i]->GetPreCopyBarrier());
            dirtyIndices.push_back(i);
        }
    }

    if (dirtyIndices.empty()) {
        return;
    }

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    for (size_t idx : dirtyIndices) {
        m_meshes[idx]->RecordCopy(cmdList, frameIndex);
    }

    barriers.clear();
    for (size_t idx : dirtyIndices) {
        barriers.push_back(m_meshes[idx]->GetPostCopyBarrier());
    }

    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}

void MeshRenderProxy::DrawAll(ID3D12GraphicsCommandList* cmdList) const {
    for (const auto& mesh : m_meshes) {
        if (mesh) {
            mesh->Draw(cmdList);
        }
    }
}

void MeshRenderProxy::DrawOne(ID3D12GraphicsCommandList* cmdList, size_t meshIndex) const {
    if (meshIndex < m_meshes.size() && m_meshes[meshIndex]) {
        m_meshes[meshIndex]->Draw(cmdList);
    }
}
