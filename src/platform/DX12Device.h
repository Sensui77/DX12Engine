#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#define MAX_FRAMES_IN_FLIGHT 2

using Microsoft::WRL::ComPtr;

class DX12Device {
public:
    DX12Device() = default;
    ~DX12Device();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();
    void Resize(int width, int height);

    // Getters for raw DOD usage
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    IDXGISwapChain3* GetSwapChain() const { return m_swapChain.Get(); }
    ID3D12CommandQueue* GetDirectQueue() const { return m_commandQueue.Get(); }
    
    // SwapChain
    ID3D12Resource* GetCurrentBackBuffer() const { return m_renderTargets[m_frameIndex].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const;
    
    // GBuffer RTVs and SRVs
    ID3D12Resource* GetAlbedoTarget() const { return m_albedoTarget.Get(); }
    ID3D12Resource* GetNormalTarget() const { return m_normalTarget.Get(); }
    
    void GetGBufferHandles(D3D12_CPU_DESCRIPTOR_HANDLE outHandles[2]) const;
    ID3D12DescriptorHeap* GetGBufferSrvHeap() const { return m_srvHeap.Get(); }
    ID3D12DescriptorHeap* GetUiSrvHeap() const { return m_uiSrvHeap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const;

    void SwapBuffers();
    int GetFrameIndex() const { return m_frameIndex; }
    void PrintDebugMessages();
    
    // GPU Info
    const wchar_t* GetAdapterName() const { return m_adapterName; }
    size_t GetDedicatedVideoMemory() const { return m_dedicatedVideoMemory; }

private:
    bool CreateFactoryAndDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRTVs();
    bool InitGBufferTargets(int width, int height);

    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    
    // SwapChain
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12Resource> m_renderTargets[MAX_FRAMES_IN_FLIGHT];
    UINT m_rtvDescriptorSize = 0;
    int m_frameIndex = 0;

    // G-Buffer 
    ComPtr<ID3D12DescriptorHeap> m_gBufferHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap; // Heap for reading textures
    ComPtr<ID3D12DescriptorHeap> m_uiSrvHeap; // Dedicated heap for ImGui
    ComPtr<ID3D12Resource> m_albedoTarget;
    ComPtr<ID3D12Resource> m_normalTarget;

    // Depth/Stencil 
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencilTarget;

    // GPU Info cache
    wchar_t m_adapterName[128] = L"Unknown";
    size_t m_dedicatedVideoMemory = 0;
};
