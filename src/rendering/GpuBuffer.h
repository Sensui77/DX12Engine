#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>

class GpuBuffer {
public:
    GpuBuffer() = default;
    ~GpuBuffer() { Release(); }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&&) = default;
    GpuBuffer& operator=(GpuBuffer&&) = default;

    // Create on UPLOAD heap (CPU-writable, persistently mapped). Used for staging.
    bool CreateUpload(ID3D12Device* device, uint32_t bufferSize, const void* initialData = nullptr) {
        Release();
        m_bufferSize = (bufferSize + 255u) & ~255u;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = m_bufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_resource));
        if (FAILED(hr)) return false;

        hr = m_resource->Map(0, nullptr, &m_mappedData);
        if (FAILED(hr)) return false;

        if (initialData && m_mappedData) {
            memcpy(m_mappedData, initialData, bufferSize);
        }
        return true;
    }

    // Create on DEFAULT heap (GPU-optimal, not CPU-accessible).
    bool CreateDefault(ID3D12Device* device, uint32_t bufferSize) {
        Release();
        m_bufferSize = (bufferSize + 255u) & ~255u;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = m_bufferSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&m_resource));
        return SUCCEEDED(hr);
    }

    // Write data to mapped UPLOAD buffer (staging only)
    void WriteData(const void* data, uint32_t byteSize, uint32_t byteOffset = 0) {
        if (m_mappedData) {
            memcpy(static_cast<uint8_t*>(m_mappedData) + byteOffset, data, byteSize);
        }
    }

    void Release() {
        if (m_mappedData && m_resource) {
            m_resource->Unmap(0, nullptr);
            m_mappedData = nullptr;
        }
        m_resource.Reset();
        m_bufferSize = 0;
    }

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }
    uint32_t GetBufferSize() const { return m_bufferSize; }
    bool IsValid() const { return m_resource != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    void* m_mappedData = nullptr;
    uint32_t m_bufferSize = 0;
};
