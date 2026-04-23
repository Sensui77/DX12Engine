#pragma once

#include <memory>

#include "CommandContext.h"
#include "CoreWindow.h"
#include "CopyEngine.h"
#include "DX12Device.h"
#include "FrameProfiler.h"
#include "GpuProfiler.h"
#include "ImGuiLayer.h"
#include "JobSystem.h"
#include "Renderer.h"
#include "runtime/IGame.h"

class Application {
public:
    Application() = default;
    ~Application();

    int Run();

private:
    bool Initialize();
    bool Tick();
    void Shutdown();
    void UpdateDeltaTime();
    void ConfigureProcessIO();
    bool InitializePlatform();
    bool InitializeRendering();
    bool InitializeTerrain();
    bool InitializeEditor();
    void InitializeTiming();
    void BeginFrame();
    bool UpdateEditor();
    void UpdateTerrain();
    void RenderFrame();
    void WaitForGpuIfNeeded();
    void ShutdownEditor();
    void ShutdownRendering();
    void ShutdownTerrain();
    void ShutdownPlatform();

    CoreWindow m_window;
    DX12Device m_device;
    CommandContext m_context;
    Renderer m_renderer;
    ImGuiLayer m_imguiLayer;
    CopyEngine m_copyEngine;
    std::unique_ptr<IGame> m_game;
    std::unique_ptr<JobSystem> m_jobSystem;
    FrameProfiler m_cpuProfiler;
    GpuProfiler m_gpuProfiler;

    LARGE_INTEGER m_frequency = {};
    LARGE_INTEGER m_lastTime = {};
    LARGE_INTEGER m_currentTime = {};
    float m_deltaTime = 0.0f;
    int m_exitCode = 0;

    bool m_windowInitialized = false;
    bool m_deviceInitialized = false;
    bool m_contextInitialized = false;
    bool m_shaderCompilerInitialized = false;
    bool m_imguiInitialized = false;
    bool m_copyEngineInitialized = false;

    bool m_windowClosed = false;
};
