#pragma once

#include <array>
#include <cstdint>

class Camera;
class CommandContext;
class CoreWindow;
class DX12Device;
struct FrameProfiler;
class GpuProfiler;
class TerrainSystem;

struct TerrainEditorUIResult {
    bool shouldContinue = true;
    bool needsTerrainUpdate = false;
};

class TerrainEditorUI {
public:
    TerrainEditorUI();

    void Initialize(const CoreWindow& window, Camera& camera);
    void HandleInput(const CoreWindow& window, Camera& camera);
    void UpdateCamera(const CoreWindow& window, Camera& camera, float deltaTime, bool wantsCaptureMouse);

    TerrainEditorUIResult Draw(TerrainSystem& terrainSystem,
                               CommandContext& context,
                               DX12Device& device,
                               const FrameProfiler& cpuProfiler,
                               const GpuProfiler& gpuProfiler,
                               float deltaTime);

private:
    bool m_cursorFree = false;
    bool m_showUI = false;
    bool m_escWasPressed = false;
    float m_terrainChunksPerSec = 0.0f;
    float m_terrainRateSampleTime = 0.0f;
    uint64_t m_lastAppliedChunkTotal = 0;
    std::array<char, 64> m_saveFileName{};
    int m_selectedProfileIdx = -1;
};
