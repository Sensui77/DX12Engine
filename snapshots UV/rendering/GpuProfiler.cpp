#include "GpuProfiler.h"
#include <iostream>

bool GpuProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    // Get GPU timestamp frequency (ticks per second)
    if (FAILED(queue->GetTimestampFrequency(&m_gpuFrequency))) {
        std::cerr << "[GpuProfiler] Failed to get timestamp frequency\n";
        return false;
    }

    // Create timestamp query heap
    D3D12_QUERY_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    heapDesc.Count = QUERY_COUNT;
    heapDesc.NodeMask = 0;

    if (FAILED(device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(&m_queryHeap)))) {
        std::cerr << "[GpuProfiler] Failed to create query heap\n";
        return false;
    }

    // Create readback buffer to resolve query results
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(uint64_t) * QUERY_COUNT;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&m_readbackBuffer)))) {
        std::cerr << "[GpuProfiler] Failed to create readback buffer\n";
        return false;
    }

    std::cout << "[GpuProfiler] Initialized (GPU freq: "
              << m_gpuFrequency << " Hz)\n";
    return true;
}

void GpuProfiler::BeginFrame() {
    if (!m_hasValidData) return;

    // Read resolved timestamps from last frame
    uint64_t* data = nullptr;
    D3D12_RANGE readRange = { 0, sizeof(uint64_t) * QUERY_COUNT };
    if (SUCCEEDED(m_readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data)))) {
        auto ticksToMs = [&](uint64_t start, uint64_t end) -> float {
            if (end <= start) return 0.0f;
            return static_cast<float>(end - start) * 1000.0f
                   / static_cast<float>(m_gpuFrequency);
        };

        gbufferGpuMs  = ticksToMs(data[QUERY_GBUFFER_START],  data[QUERY_GBUFFER_END]);
        lightingGpuMs = ticksToMs(data[QUERY_LIGHTING_START], data[QUERY_LIGHTING_END]);
        uploadGpuMs   = ticksToMs(data[QUERY_UPLOAD_START],   data[QUERY_UPLOAD_END]);

        D3D12_RANGE writeRange = { 0, 0 };
        m_readbackBuffer->Unmap(0, &writeRange);
    }
}

void GpuProfiler::MarkUploadStart(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_UPLOAD_START);
}

void GpuProfiler::MarkUploadEnd(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_UPLOAD_END);
}

void GpuProfiler::MarkGBufferStart(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_GBUFFER_START);
}

void GpuProfiler::MarkGBufferEnd(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_GBUFFER_END);
}

void GpuProfiler::MarkLightingStart(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_LIGHTING_START);
}

void GpuProfiler::MarkLightingEnd(ID3D12GraphicsCommandList* cmdList) {
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, QUERY_LIGHTING_END);
}

void GpuProfiler::EndFrame(ID3D12GraphicsCommandList* cmdList) {
    // Resolve all queries into the readback buffer
    cmdList->ResolveQueryData(
        m_queryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        0, QUERY_COUNT,
        m_readbackBuffer.Get(), 0);

    m_hasValidData = true;
}
