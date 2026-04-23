#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class GraphicsPipeline {
public:
    GraphicsPipeline() = default;
    ~GraphicsPipeline() = default;

    bool Initialize(ID3D12Device* device, const std::wstring& shaderFile);

    ID3D12PipelineState* GetPSO() const { return m_pipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    bool CreateRootSignature(ID3D12Device* device);
};
