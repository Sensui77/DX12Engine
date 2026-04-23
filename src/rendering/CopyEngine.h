#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// Manages a dedicated D3D12 COPY command queue for asynchronous GPU uploads.
// Currently supports batch mode (Begin + record + ExecuteAndWait).
// Can be extended with per-frame BeginFrame/CloseAndExecute for streaming uploads.
class CopyEngine {
public:
    CopyEngine() = default;
    ~CopyEngine();

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    ID3D12CommandQueue* GetQueue() const { return m_queue.Get(); }
    ID3D12Fence* GetFence() const { return m_fence.Get(); }
    uint64_t GetFenceValue() const { return m_fenceValue; }

    // Batch mode: call Begin(), record copy commands on the returned list,
    // then call ExecuteAndWait() to submit and synchronously wait for completion.
    ID3D12GraphicsCommandList* Begin();
    bool ExecuteAndWait();

private:
    ID3D12Device* m_device = nullptr;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<ID3D12CommandAllocator> m_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_cmdList;
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;
    bool m_cmdListCreated = false;
};
