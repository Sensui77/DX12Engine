#include "DX12Device.h"
#include <iostream>
#include <d3d12sdklayers.h>

#define CHECK_HR(hr) if (FAILED(hr)) return false;

DX12Device::~DX12Device() {
    Shutdown();
}

bool DX12Device::Initialize(HWND hwnd, int width, int height) {
    if (!CreateFactoryAndDevice()) return false;
    if (!CreateCommandQueue()) return false;
    if (!CreateSwapChain(hwnd, width, height)) return false;
    if (!CreateRTVs()) return false;
    
    if (!InitGBufferTargets(width, height)) return false;

    return true;
}

void DX12Device::Shutdown() {
}

void DX12Device::Resize(int width, int height) {
    if (width == 0 || height == 0) return;

    // Liberar Buffers
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) m_renderTargets[i].Reset();
    m_albedoTarget.Reset();
    m_normalTarget.Reset();
    m_depthStencilTarget.Reset();

    // Liberar Heaps
    m_rtvHeap.Reset();
    m_gBufferHeap.Reset();
    m_srvHeap.Reset();
    m_uiSrvHeap.Reset();
    m_dsvHeap.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(MAX_FRAMES_IN_FLIGHT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
        std::cerr << "[DX12] Falha ao redimensionar SwapChain. HR: " << std::hex << hr << "\n" << std::flush;
        return;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    CreateRTVs();
    InitGBufferTargets(width, height);
}

bool DX12Device::CreateFactoryAndDevice() {
    UINT dxgiFactoryFlags = 0;

    // Debug Layer — always enabled for diagnostics
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        std::cout << "[DX12] Debug Layer ATIVADO\n" << std::flush;
    }

    CHECK_HR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));
    CHECK_HR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

    // Cache GPU Info
    ComPtr<IDXGIAdapter1> adapter;
    if (SUCCEEDED(m_factory->EnumAdapters1(0, &adapter))) {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            wcscpy_s(m_adapterName, desc.Description);
            m_dedicatedVideoMemory = desc.DedicatedVideoMemory;
        }
    }

    // Set up InfoQueue to break on errors
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // Don't break - just capture messages for stderr output
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, FALSE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
        std::cout << "[DX12] InfoQueue configurado (break on error)\n" << std::flush;
    }

    return true;
}

bool DX12Device::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CHECK_HR(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    return true;
}

bool DX12Device::CreateSwapChain(HWND hwnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = MAX_FRAMES_IN_FLIGHT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> tempSwapChain;
    CHECK_HR(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain
    ));

    CHECK_HR(tempSwapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return true;
}

bool DX12Device::CreateRTVs() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = MAX_FRAMES_IN_FLIGHT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CHECK_HR(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT n = 0; n < MAX_FRAMES_IN_FLIGHT; n++) {
        CHECK_HR(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
    return true;
}

bool DX12Device::InitGBufferTargets(int width, int height) {
    // 1. RTV Heap para GBuffer
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2; 
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CHECK_HR(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_gBufferHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CHECK_HR(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    // 2. SRV Heap para Deferred Lighting (Albedo e Normal)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Visível pro Shader!
    CHECK_HR(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    // 3. SRV Heap para ImGui (Dedicated)
    D3D12_DESCRIPTOR_HEAP_DESC uiSrvHeapDesc = {};
    uiSrvHeapDesc.NumDescriptors = 1;
    uiSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    uiSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CHECK_HR(m_device->CreateDescriptorHeap(&uiSrvHeapDesc, IID_PPV_ARGS(&m_uiSrvHeap)));

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    
    // Albedo
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_CLEAR_VALUE clearAlbedo = {};
    clearAlbedo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearAlbedo.Color[0] = 0.0f; clearAlbedo.Color[1] = 0.0f; clearAlbedo.Color[2] = 0.0f; clearAlbedo.Color[3] = 1.0f;
    CHECK_HR(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearAlbedo, 
        IID_PPV_ARGS(&m_albedoTarget)));

    // Normal
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    D3D12_CLEAR_VALUE clearNormal = {};
    clearNormal.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    clearNormal.Color[0] = 0.0f; clearNormal.Color[1] = 0.0f; clearNormal.Color[2] = 0.0f; clearNormal.Color[3] = 1.0f;
    CHECK_HR(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearNormal, 
        IID_PPV_ARGS(&m_normalTarget)));

    // Create RTV Views
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_gBufferHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateRenderTargetView(m_albedoTarget.Get(), nullptr, rtvHandle);
    rtvHandle.ptr += m_rtvDescriptorSize;
    m_device->CreateRenderTargetView(m_normalTarget.Get(), nullptr, rtvHandle);

    // Create SRV Views
    UINT srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    // Albedo SRV
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_device->CreateShaderResourceView(m_albedoTarget.Get(), &srvDesc, srvHandle);

    // Normal SRV
    srvHandle.ptr += srvDescriptorSize;
    srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    m_device->CreateShaderResourceView(m_normalTarget.Get(), &srvDesc, srvHandle);

    // Create Depth Stencil Resource
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearDepth = {};
    clearDepth.Format = DXGI_FORMAT_D32_FLOAT;
    clearDepth.DepthStencil.Depth = 1.0f;
    clearDepth.DepthStencil.Stencil = 0;

    CHECK_HR(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearDepth,
        IID_PPV_ARGS(&m_depthStencilTarget)));

    // Create DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_depthStencilTarget.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::GetCurrentRtvHandle() const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += m_frameIndex * m_rtvDescriptorSize;
    return handle;
}

void DX12Device::GetGBufferHandles(D3D12_CPU_DESCRIPTOR_HANDLE outHandles[2]) const {
    outHandles[0] = m_gBufferHeap->GetCPUDescriptorHandleForHeapStart();
    outHandles[1] = outHandles[0];
    outHandles[1].ptr += m_rtvDescriptorSize;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12Device::GetDsvHandle() const {
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void DX12Device::SwapBuffers() {
    m_swapChain->Present(1, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Device::PrintDebugMessages() {
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) return;

    UINT64 messageCount = infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < messageCount; i++) {
        SIZE_T messageLength = 0;
        infoQueue->GetMessage(i, nullptr, &messageLength);
        
        D3D12_MESSAGE* pMessage = (D3D12_MESSAGE*)malloc(messageLength);
        infoQueue->GetMessage(i, pMessage, &messageLength);
        
        const char* severity = "INFO";
        if (pMessage->Severity == D3D12_MESSAGE_SEVERITY_ERROR) severity = "ERROR";
        else if (pMessage->Severity == D3D12_MESSAGE_SEVERITY_WARNING) severity = "WARN";
        else if (pMessage->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) severity = "CORRUPTION";
        
        std::cerr << "[DX12:" << severity << "] " << pMessage->pDescription << "\n";
        free(pMessage);
    }
    if (messageCount > 0) std::cerr << std::flush;
    infoQueue->ClearStoredMessages();
}
