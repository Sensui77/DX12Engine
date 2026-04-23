#pragma once

#include "Camera.h"
#include "TerrainEditorUI.h"
#include "TerrainSystem.h"
#include "runtime/IGame.h"

class InteractiveTerrainGame : public IGame {
public:
    InteractiveTerrainGame() = default;
    ~InteractiveTerrainGame() override = default;

    bool Initialize(const GameInitializeContext& initContext) override;
    Camera& GetCamera() override { return m_camera; }
    void HandleInput(const CoreWindow& window) override;
    void Update(const GameUpdateContext& updateContext) override;
    bool DrawUi(const GameUiContext& uiContext) override;
    void UpdateSimulation(uint32_t frameIndex) override;
    void RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) override;
    void DrawScene(ID3D12GraphicsCommandList* cmdList, const Frustum& frustum) override;

private:
    Camera m_camera;
    TerrainSystem m_terrainSystem;
    TerrainEditorUI m_terrainEditorUI;
};
