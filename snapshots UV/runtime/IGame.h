#pragma once

#include <cstdint>

class Camera;
class CommandContext;
class CoreWindow;
class CopyEngine;
class DX12Device;
class FrameProfiler;
class Frustum;
class GpuProfiler;
class JobSystem;
struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

struct GameInitializeContext {
    CoreWindow& window;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* directQueue = nullptr;
    CopyEngine& copyEngine;
    JobSystem& jobSystem;
};

struct GameUpdateContext {
    const CoreWindow& window;
    float deltaTime = 0.0f;
    bool wantsCaptureMouse = false;
};

struct GameUiContext {
    CommandContext& commandContext;
    DX12Device& device;
    const FrameProfiler& cpuProfiler;
    const GpuProfiler& gpuProfiler;
    float deltaTime = 0.0f;
};

class IGame {
public:
    virtual ~IGame() = default;

    virtual bool Initialize(const GameInitializeContext& initContext) = 0;
    virtual Camera& GetCamera() = 0;
    virtual void HandleInput(const CoreWindow& window) = 0;
    virtual void Update(const GameUpdateContext& updateContext) = 0;
    virtual bool DrawUi(const GameUiContext& uiContext) = 0;
    virtual void UpdateSimulation(uint32_t frameIndex) = 0;
    virtual void RecordUploads(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) = 0;
    virtual void DrawScene(ID3D12GraphicsCommandList* cmdList, const Frustum& frustum) = 0;
};
