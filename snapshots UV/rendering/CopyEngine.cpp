#include "CopyEngine.h"
#include <iostream>

CopyEngine::~CopyEngine() {
    Shutdown();
}

bool CopyEngine::Initialize(ID3D12Device* device) {
    m_device = device;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue)))) {
        std::cerr << "[CopyEngine] Failed to create COPY command queue\n";
        return false;
    }

    if (FAILED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_allocator)))) {
        std::cerr << "[CopyEngine] Failed to create command allocator\n";
        return false;
    }

    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        std::cerr << "[CopyEngine] Failed to create fence\n";
        return false;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        std::cerr << "[CopyEngine] Failed to create fence event\n";
        return false;
    }

    std::cout << "[CopyEngine] COPY queue initialized\n";
    return true;
}

void CopyEngine::Shutdown() {
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

ID3D12GraphicsCommandList* CopyEngine::Begin() {
    m_allocator->Reset();

    if (!m_cmdListCreated) {
        if (FAILED(m_device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_COPY,
                m_allocator.Get(), nullptr,
                IID_PPV_ARGS(&m_cmdList)))) {
            std::cerr << "[CopyEngine] Failed to create command list\n";
            return nullptr;
        }
        m_cmdListCreated = true;
    } else {
        m_cmdList->Reset(m_allocator.Get(), nullptr);
    }

    return m_cmdList.Get();
}

bool CopyEngine::ExecuteAndWait() {
    if (FAILED(m_cmdList->Close())) {
        std::cerr << "[CopyEngine] Failed to close command list\n";
        return false;
    }

    ID3D12CommandList* lists[] = { m_cmdList.Get() };
    m_queue->ExecuteCommandLists(1, lists);

    m_fenceValue++;
    m_queue->Signal(m_fence.Get(), m_fenceValue);

    if (m_fence->GetCompletedValue() < m_fenceValue) {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    return true;
}
