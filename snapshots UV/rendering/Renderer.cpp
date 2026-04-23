#include "Renderer.h"

#include "Camera.h"
#include "CommandContext.h"
#include "CoreWindow.h"
#include "DX12Device.h"
#include "FrameProfiler.h"
#include "Frustum.h"
#include "GpuProfiler.h"
#include "ImGuiLayer.h"
#include "GraphicsPipeline.h"
#include "DeferredPipeline.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"

#include <DirectXMath.h>
Renderer::~Renderer() = default;

bool Renderer::Initialize(ID3D12Device* device,
                          int width,
                          int height,
                          const std::wstring& terrainShader,
                          const std::wstring& deferredShader) {
    m_graphicsPipeline = std::make_unique<GraphicsPipeline>();
    if (!m_graphicsPipeline->Initialize(device, terrainShader)) {
        m_graphicsPipeline.reset();
        return false;
    }

    m_deferredPipeline = std::make_unique<DeferredPipeline>();
    if (!m_deferredPipeline->Initialize(device, deferredShader)) {
        m_graphicsPipeline.reset();
        m_deferredPipeline.reset();
        return false;
    }

    SetViewportSize(width, height);
    return true;
}

void Renderer::HandleResizeIfNeeded(CommandContext& context,
                                    DX12Device& device,
                                    CoreWindow& window,
                                    Camera& camera,
                                    ImGuiLayer& imguiLayer) {
    if (!window.WasResized()) {
        return;
    }

    context.WaitForGPU(device.GetDirectQueue());

    const int width = window.GetWidth();
    const int height = window.GetHeight();
    if (width > 0 && height > 0) {
        imguiLayer.InvalidateDeviceObjects();
        device.Resize(width, height);
        camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
        SetViewportSize(width, height);
        imguiLayer.CreateDeviceObjects(window, device);
    }

    window.ResetResizeFlag();
}

void Renderer::RenderFrame(CommandContext& context,
                           DX12Device& device,
                           const Camera& camera,
                           FrameProfiler& cpuProfiler,
                           GpuProfiler& gpuProfiler,
                           const RenderFrameCallbacks& callbacks) {
    const uint32_t frameIndex = static_cast<uint32_t>(device.GetFrameIndex());
    context.BeginFrame(frameIndex);
    ID3D12GraphicsCommandList* cmdList = context.GetCommandList();

    gpuProfiler.BeginFrame();

    cpuProfiler.BeginTerrainUpload();
    gpuProfiler.MarkUploadStart(cmdList);
    if (callbacks.recordUploads) {
        callbacks.recordUploads(cmdList, frameIndex);
    }
    gpuProfiler.MarkUploadEnd(cmdList);
    cpuProfiler.EndTerrainUpload();

    cpuProfiler.BeginGBufferPass();
    gpuProfiler.MarkGBufferStart(cmdList);

    cmdList->SetPipelineState(m_graphicsPipeline->GetPSO());
    cmdList->SetGraphicsRootSignature(m_graphicsPipeline->GetRootSignature());
    DirectX::XMMATRIX vpMatrix = camera.GetViewProjectionMatrix();
    DirectX::XMMATRIX vpTransposed = DirectX::XMMatrixTranspose(vpMatrix);
    cmdList->SetGraphicsRoot32BitConstants(0, 16, &vpTransposed, 0);

    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE gbufferHandles[2];
    device.GetGBufferHandles(gbufferHandles);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = device.GetDsvHandle();

    cmdList->OMSetRenderTargets(2, gbufferHandles, FALSE, &dsvHandle);
    const float clearZero[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    cmdList->ClearRenderTargetView(gbufferHandles[0], clearZero, 0, nullptr);
    cmdList->ClearRenderTargetView(gbufferHandles[1], clearZero, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Frustum frustum;
    frustum.ExtractFromVP(vpMatrix);
    if (callbacks.drawScene) {
        callbacks.drawScene(cmdList, frustum);
    }

    gpuProfiler.MarkGBufferEnd(cmdList);
    cpuProfiler.EndGBufferPass();

    D3D12_RESOURCE_BARRIER barriersToSRV[2] = {};
    barriersToSRV[0] = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetAlbedoTarget(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE}};
    barriersToSRV[1] = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetNormalTarget(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE}};
    cmdList->ResourceBarrier(2, barriersToSRV);

    D3D12_RESOURCE_BARRIER presentToRT = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetCurrentBackBuffer(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET}};
    cmdList->ResourceBarrier(1, &presentToRT);

    cpuProfiler.BeginLightingPass();
    gpuProfiler.MarkLightingStart(cmdList);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvH = device.GetCurrentRtvHandle();
    cmdList->OMSetRenderTargets(1, &rtvH, FALSE, nullptr);
    const float clearScreen[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    cmdList->ClearRenderTargetView(rtvH, clearScreen, 0, nullptr);

    cmdList->SetPipelineState(m_deferredPipeline->GetPSO());
    cmdList->SetGraphicsRootSignature(m_deferredPipeline->GetRootSignature());
    ID3D12DescriptorHeap* descriptorHeaps[] = { device.GetGBufferSrvHeap() };
    cmdList->SetDescriptorHeaps(1, descriptorHeaps);
    cmdList->SetGraphicsRootDescriptorTable(0, device.GetGBufferSrvHeap()->GetGPUDescriptorHandleForHeapStart());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    gpuProfiler.MarkLightingEnd(cmdList);
    cpuProfiler.EndLightingPass();

    cpuProfiler.BeginImGuiPass();
    ID3D12DescriptorHeap* uiHeaps[] = { device.GetUiSrvHeap() };
    cmdList->SetDescriptorHeaps(1, uiHeaps);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
    cpuProfiler.EndImGuiPass();

    D3D12_RESOURCE_BARRIER barriersToRTV[2] = {};
    barriersToRTV[0] = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetAlbedoTarget(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET}};
    barriersToRTV[1] = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetNormalTarget(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET}};
    cmdList->ResourceBarrier(2, barriersToRTV);

    D3D12_RESOURCE_BARRIER rtToPresent = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {device.GetCurrentBackBuffer(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT}};
    cmdList->ResourceBarrier(1, &rtToPresent);

    gpuProfiler.EndFrame(cmdList);

    context.CloseAndExecute(device.GetDirectQueue(), frameIndex);
    device.SwapBuffers();
}

void Renderer::SetViewportSize(int width, int height) {
    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, width, height };
}
