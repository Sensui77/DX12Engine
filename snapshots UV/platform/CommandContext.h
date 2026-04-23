#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class CommandContext {
public:
    CommandContext() = default;
    ~CommandContext();

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

    // Per-frame sync: waits for GPU to finish this frame's previous work, then resets allocator
    void BeginFrame(uint32_t frameIndex);

    // Close command list, execute on queue, signal fence for this frame
    void CloseAndExecute(ID3D12CommandQueue* queue, uint32_t frameIndex);

    // Full GPU stall — use only at shutdown and resize
    void WaitForGPU(ID3D12CommandQueue* queue);

private:
    static constexpr int MAX_FRAMES = 3;

    ID3D12Device* m_device = nullptr;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[MAX_FRAMES];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Fence> m_fence;

    // Global fence counter — always increasing, never reused
    uint64_t m_fenceValue = 0;
    // The fence value that was signaled when each frame last finished GPU work
    uint64_t m_frameFenceValues[MAX_FRAMES] = {};

    HANDLE m_fenceEvent = nullptr;
    bool m_commandListCreated = false;
};
