#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include "GpuBuffer.h"

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 2
#endif

class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Creates DEFAULT heap vertex/index buffers + UPLOAD staging ring.
    // Does NOT perform GPU upload. Caller must use the batch upload protocol:
    //   1. RecordInitialCopy(copyCmdList)     — on a COPY command list
    //   2. Execute the COPY command list
    //   3. RecordTransitionToRenderState(directCmdList) — on a DIRECT command list
    //   4. Execute the DIRECT command list
    //   5. FinalizeInitialUpload()            — release temp staging
    bool Initialize(ID3D12Device* device,
                    uint32_t vertexStride,
                    const void* vertexData, uint32_t vertexCount,
                    const uint32_t* indexData, uint32_t indexCount);

    // Record CopyBufferRegion commands for initial vertex + index data.
    // Must be called on a COPY command list. Staging kept alive until Finalize.
    void RecordInitialCopy(ID3D12GraphicsCommandList* cmdList);

    // Transition buffers from COMMON (post-COPY-queue decay) to render states.
    // Must be called on a DIRECT command list after the copy batch completes.
    void RecordTransitionToRenderState(ID3D12GraphicsCommandList* cmdList);

    // Release temporary staging buffers after GPU has completed both batches.
    void FinalizeInitialUpload();

    // Per-frame: stage vertex data into the UPLOAD ring buffer for this frame.
    void StageVertices(const void* data, uint32_t byteSize, uint32_t frameIndex);

    // Batched per-frame upload (3-phase pattern used by terrain render proxies):
    bool NeedsUpload(uint32_t frameIndex) const;
    D3D12_RESOURCE_BARRIER GetPreCopyBarrier() const;
    void RecordCopy(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex);
    D3D12_RESOURCE_BARRIER GetPostCopyBarrier();

    void Draw(ID3D12GraphicsCommandList* cmdList) const;

    uint32_t GetIndexCount() const { return m_indexCount; }
    uint32_t GetVertexCount() const { return m_vertexCount; }

private:
    GpuBuffer m_vertexBuffer;                           // DEFAULT heap
    GpuBuffer m_indexBuffer;                            // DEFAULT heap
    GpuBuffer m_vertexStaging[MAX_FRAMES_IN_FLIGHT];    // UPLOAD ring

    bool m_dirtyFrames[MAX_FRAMES_IN_FLIGHT] = {};

    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    uint32_t m_vertexStride = 0;
    uint32_t m_vertexByteSize = 0;
    uint32_t m_indexByteSize = 0;

    D3D12_RESOURCE_STATES m_vertexState = D3D12_RESOURCE_STATE_COMMON;

    // Temporary staging for batch initial upload (released by FinalizeInitialUpload)
    struct PendingUpload {
        GpuBuffer vertexStaging;
        GpuBuffer indexStaging;
    };
    std::unique_ptr<PendingUpload> m_pendingUpload;
};
