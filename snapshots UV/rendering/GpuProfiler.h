#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

using Microsoft::WRL::ComPtr;

// GPU timestamp profiler using ID3D12QueryHeap.
// Measures GPU time per render pass via timestamp queries resolved to a readback buffer.
//
// Query slots layout (2 timestamps per pass):
//   0,1 = G-Buffer pass
//   2,3 = Lighting pass
//   4,5 = Upload/Copy pass
class GpuProfiler {
public:
    static constexpr uint32_t QUERY_GBUFFER_START  = 0;
    static constexpr uint32_t QUERY_GBUFFER_END    = 1;
    static constexpr uint32_t QUERY_LIGHTING_START = 2;
    static constexpr uint32_t QUERY_LIGHTING_END   = 3;
    static constexpr uint32_t QUERY_UPLOAD_START   = 4;
    static constexpr uint32_t QUERY_UPLOAD_END     = 5;
    static constexpr uint32_t QUERY_COUNT          = 6;

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);

    // Call once per frame before recording passes (reads previous frame results)
    void BeginFrame();

    // Insert timestamp queries into the command list
    void MarkUploadStart(ID3D12GraphicsCommandList* cmdList);
    void MarkUploadEnd(ID3D12GraphicsCommandList* cmdList);
    void MarkGBufferStart(ID3D12GraphicsCommandList* cmdList);
    void MarkGBufferEnd(ID3D12GraphicsCommandList* cmdList);
    void MarkLightingStart(ID3D12GraphicsCommandList* cmdList);
    void MarkLightingEnd(ID3D12GraphicsCommandList* cmdList);

    // Call at end of frame — resolves queries to readback buffer
    void EndFrame(ID3D12GraphicsCommandList* cmdList);

    // Results (available next frame, in milliseconds)
    float gbufferGpuMs = 0.0f;
    float lightingGpuMs = 0.0f;
    float uploadGpuMs = 0.0f;

private:
    ComPtr<ID3D12QueryHeap> m_queryHeap;
    ComPtr<ID3D12Resource> m_readbackBuffer;
    uint64_t m_gpuFrequency = 1;
    bool m_hasValidData = false;
};
