#include "CommandContext.h"
#include <algorithm>

CommandContext::~CommandContext() {
    Shutdown();
}

bool CommandContext::Initialize(ID3D12Device* device) {
    m_device = device;

    // Create one allocator per frame in flight
    for (uint32_t i = 0; i < MAX_FRAMES; i++) {
        if (FAILED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])))) {
            return false;
        }
    }

    // Create fence
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        return false;
    }

    m_fenceValue = 0;
    for (uint32_t i = 0; i < MAX_FRAMES; i++) {
        m_frameFenceValues[i] = 0;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) return false;

    // Command list is created lazily in the first BeginFrame call.
    // This avoids the debug-layer error from creating+closing a command list
    // with an allocator and then resetting that allocator before executing.

    return true;
}

void CommandContext::Shutdown() {
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

void CommandContext::BeginFrame(uint32_t frameIndex) {
    // Wait until GPU has finished with this frame's previous work
    if (m_fence->GetCompletedValue() < m_frameFenceValues[frameIndex]) {
        m_fence->SetEventOnCompletion(m_frameFenceValues[frameIndex], m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Now safe to reset this frame's allocator
    m_commandAllocators[frameIndex]->Reset();

    if (!m_commandListCreated) {
        // First frame: create command list in recording state
        if (FAILED(m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[frameIndex].Get(),
            nullptr,
            IID_PPV_ARGS(&m_commandList)))) {
            return;
        }
        m_commandListCreated = true;
    } else {
        m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr);
    }
}

void CommandContext::CloseAndExecute(ID3D12CommandQueue* queue, uint32_t frameIndex) {
    m_commandList->Close();

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    queue->ExecuteCommandLists(1, ppCommandLists);

    // Signal fence with a monotonically increasing value.
    // Each frame gets a unique value so BeginFrame can wait for the EXACT
    // GPU submission that used this frame's allocator.
    m_fenceValue++;
    m_frameFenceValues[frameIndex] = m_fenceValue;
    queue->Signal(m_fence.Get(), m_fenceValue);
}

void CommandContext::WaitForGPU(ID3D12CommandQueue* queue) {
    // Signal a new fence value beyond all pending work and wait
    m_fenceValue++;
    queue->Signal(m_fence.Get(), m_fenceValue);

    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // All GPU work is complete — sync per-frame fence values
    for (uint32_t i = 0; i < MAX_FRAMES; i++) {
        m_frameFenceValues[i] = m_fenceValue;
    }
}
