#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>

#include <functional>
#include <memory>
#include <string>

#include "DeferredPipeline.h"
#include "GraphicsPipeline.h"

class Camera;
class CommandContext;
class DX12Device;
class ImGuiLayer;
class CoreWindow;
struct FrameProfiler;
class GpuProfiler;
class Frustum;

struct RenderFrameCallbacks {
    std::function<void(ID3D12GraphicsCommandList*, uint32_t)> recordUploads;
    std::function<void(ID3D12GraphicsCommandList*, const Frustum&)> drawScene;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool Initialize(ID3D12Device* device,
                    int width,
                    int height,
                    const std::wstring& terrainShader,
                    const std::wstring& deferredShader);

    void HandleResizeIfNeeded(CommandContext& context,
                              DX12Device& device,
                              CoreWindow& window,
                              Camera& camera,
                              ImGuiLayer& imguiLayer);

    void RenderFrame(CommandContext& context,
                     DX12Device& device,
                     const Camera& camera,
                     FrameProfiler& cpuProfiler,
                     GpuProfiler& gpuProfiler,
                     const RenderFrameCallbacks& callbacks);

private:
    void SetViewportSize(int width, int height);

    std::unique_ptr<GraphicsPipeline> m_graphicsPipeline;
    std::unique_ptr<DeferredPipeline> m_deferredPipeline;
    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT m_scissorRect = {};
};
