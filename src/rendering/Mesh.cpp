#include "Mesh.h"
#include <iostream>

bool Mesh::Initialize(ID3D12Device* device,
                      uint32_t vertexStride,
                      const void* vertexData, uint32_t vertexCount,
                      const uint32_t* indexData, uint32_t indexCount)
{
    m_vertexCount = vertexCount;
    m_indexCount = indexCount;
    m_vertexStride = vertexStride;
    m_vertexByteSize = vertexStride * vertexCount;
    m_indexByteSize = static_cast<uint32_t>(sizeof(uint32_t)) * indexCount;

    // Create DEFAULT heap buffers (GPU-optimal)
    if (!m_vertexBuffer.CreateDefault(device, m_vertexByteSize)) {
        std::cerr << "[Mesh] Failed to create DEFAULT vertex buffer\n";
        return false;
    }
    if (!m_indexBuffer.CreateDefault(device, m_indexByteSize)) {
        std::cerr << "[Mesh] Failed to create DEFAULT index buffer\n";
        return false;
    }

    // Create UPLOAD staging ring for dynamic vertex updates
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (!m_vertexStaging[i].CreateUpload(device, m_vertexByteSize)) {
            std::cerr << "[Mesh] Failed to create staging buffer " << i << "\n";
            return false;
        }
    }

    // Build views pointing to DEFAULT buffers
    m_vbv.BufferLocation = m_vertexBuffer.GetGPUVirtualAddress();
    m_vbv.StrideInBytes = vertexStride;
    m_vbv.SizeInBytes = m_vertexByteSize;

    m_ibv.BufferLocation = m_indexBuffer.GetGPUVirtualAddress();
    m_ibv.SizeInBytes = m_indexByteSize;
    m_ibv.Format = DXGI_FORMAT_R32_UINT;

    // Store initial data in temporary staging for batch upload
    m_pendingUpload = std::make_unique<PendingUpload>();
    if (!m_pendingUpload->vertexStaging.CreateUpload(device, m_vertexByteSize, vertexData)) {
        std::cerr << "[Mesh] Failed to create initial vertex staging\n";
        return false;
    }
    if (!m_pendingUpload->indexStaging.CreateUpload(device, m_indexByteSize, indexData)) {
        std::cerr << "[Mesh] Failed to create initial index staging\n";
        return false;
    }

    m_vertexState = D3D12_RESOURCE_STATE_COMMON;
    return true;
}

void Mesh::RecordInitialCopy(ID3D12GraphicsCommandList* cmdList) {
    if (!m_pendingUpload) return;

    // On COPY queue: DEFAULT buffers start in COMMON, implicitly promoted to COPY_DEST.
    // After COPY queue execution completes, buffers decay back to COMMON.
    cmdList->CopyBufferRegion(m_vertexBuffer.GetResource(), 0,
                              m_pendingUpload->vertexStaging.GetResource(), 0,
                              m_vertexByteSize);
    cmdList->CopyBufferRegion(m_indexBuffer.GetResource(), 0,
                              m_pendingUpload->indexStaging.GetResource(), 0,
                              m_indexByteSize);
}

void Mesh::RecordTransitionToRenderState(ID3D12GraphicsCommandList* cmdList) {
    // After COPY queue execution + decay, buffers are in COMMON.
    // Transition to usable render states on DIRECT queue.
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = m_vertexBuffer.GetResource();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Transition.pResource = m_indexBuffer.GetResource();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(2, barriers);
    m_vertexState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

void Mesh::FinalizeInitialUpload() {
    m_pendingUpload.reset();
}

void Mesh::StageVertices(const void* data, uint32_t byteSize, uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;
    m_vertexStaging[frameIndex].WriteData(data, byteSize);
    m_dirtyFrames[frameIndex] = true;
}

bool Mesh::NeedsUpload(uint32_t frameIndex) const {
    return frameIndex < MAX_FRAMES_IN_FLIGHT && m_dirtyFrames[frameIndex];
}

D3D12_RESOURCE_BARRIER Mesh::GetPreCopyBarrier() const {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_vertexBuffer.GetResource();
    barrier.Transition.StateBefore = m_vertexState;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}

void Mesh::RecordCopy(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT) return;

    cmdList->CopyBufferRegion(
        m_vertexBuffer.GetResource(), 0,
        m_vertexStaging[frameIndex].GetResource(), 0,
        m_vertexByteSize);

    m_dirtyFrames[frameIndex] = false;
}

D3D12_RESOURCE_BARRIER Mesh::GetPostCopyBarrier() {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_vertexBuffer.GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_vertexState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    return barrier;
}

void Mesh::Draw(ID3D12GraphicsCommandList* cmdList) const {
    cmdList->IASetVertexBuffers(0, 1, &m_vbv);
    cmdList->IASetIndexBuffer(&m_ibv);
    cmdList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}
