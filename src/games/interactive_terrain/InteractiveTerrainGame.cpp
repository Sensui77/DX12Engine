#include "games/interactive_terrain/InteractiveTerrainGame.h"

#include "CoreWindow.h"
#include "DX12Device.h"
#include "FrameProfiler.h"
#include "GpuProfiler.h"

bool InteractiveTerrainGame::Initialize(const GameInitializeContext& initContext) {
    m_camera.Initialize(
        90.0f,
        static_cast<float>(initContext.window.GetWidth()) / static_cast<float>(initContext.window.GetHeight()),
        0.1f,
        2000.0f);
    m_terrainEditorUI.Initialize(initContext.window, m_camera);

    return m_terrainSystem.Initialize(
        initContext.device,
        initContext.directQueue,
        initContext.copyEngine,
        initContext.jobSystem);
}

void InteractiveTerrainGame::HandleInput(const CoreWindow& window) {
    m_terrainEditorUI.HandleInput(window, m_camera);
}

void InteractiveTerrainGame::Update(const GameUpdateContext& updateContext) {
    m_terrainEditorUI.UpdateCamera(
        updateContext.window,
        m_camera,
        updateContext.deltaTime,
        updateContext.wantsCaptureMouse);
}

bool InteractiveTerrainGame::DrawUi(const GameUiContext& uiContext) {
    const TerrainEditorUIResult uiResult = m_terrainEditorUI.Draw(
        m_terrainSystem,
        uiContext.commandContext,
        uiContext.device,
        uiContext.cpuProfiler,
        uiContext.gpuProfiler,
        uiContext.deltaTime);

    if (!uiResult.shouldContinue) {
        return false;
    }

    if (uiResult.needsTerrainUpdate) {
        m_terrainSystem.MarkAllDirty();
    }

    return true;
}

void InteractiveTerrainGame::UpdateSimulation(uint32_t frameIndex) {
    const DirectX::XMFLOAT3 cameraPos = m_camera.GetPosition();
    m_terrainSystem.Update(cameraPos.x, cameraPos.z, frameIndex);
}

void InteractiveTerrainGame::RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
    m_terrainSystem.RecordUploads(cmdList, frameIndex);
}

void InteractiveTerrainGame::DrawScene(ID3D12GraphicsCommandList* cmdList, const Frustum& frustum) {
    m_terrainSystem.Draw(cmdList, frustum, m_terrainSystem.WorldSettings());
}
