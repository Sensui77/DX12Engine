#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Application.h"

#include <iostream>

#include "games/interactive_terrain/InteractiveTerrainGame.h"
#include "JobSystem.h"
#include "ShaderCompiler.h"

Application::~Application() {
    Shutdown();
}

int Application::Run() {
    ConfigureProcessIO();
    std::cout << "[Engine] Iniciando Antigravity Engine...\n" << std::flush;

    if (!Initialize()) {
        m_exitCode = -1;
        Shutdown();
        return m_exitCode;
    }

    while (!m_window.ShouldClose()) {
        if (!Tick()) {
            break;
        }
    }

    Shutdown();
    return m_exitCode;
}

bool Application::Initialize() {
    return InitializePlatform()
        && InitializeRendering()
        && InitializeTerrain()
        && InitializeEditor();
}

bool Application::Tick() {
    BeginFrame();

    if (!UpdateEditor()) {
        return false;
    }

    UpdateTerrain();
    RenderFrame();
    m_cpuProfiler.FrameEnd();
    return true;
}

void Application::Shutdown() {
    WaitForGpuIfNeeded();
    ShutdownEditor();
    ShutdownRendering();
    ShutdownTerrain();
    ShutdownPlatform();
}

void Application::UpdateDeltaTime() {
    QueryPerformanceCounter(&m_currentTime);
    m_deltaTime = static_cast<float>(m_currentTime.QuadPart - m_lastTime.QuadPart)
        / static_cast<float>(m_frequency.QuadPart);
    m_lastTime = m_currentTime;
}

void Application::ConfigureProcessIO() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

bool Application::InitializePlatform() {
    if (!m_window.Initialize(1280, 720, "Antigravity Engine - Procedural Controls")) {
        return false;
    }
    m_windowInitialized = true;

    if (!m_device.Initialize(m_window.GetHwnd(), m_window.GetWidth(), m_window.GetHeight())) {
        return false;
    }
    m_deviceInitialized = true;

    if (!m_context.Initialize(m_device.GetDevice())) {
        return false;
    }
    m_contextInitialized = true;

    InitializeTiming();
    return true;
}

bool Application::InitializeRendering() {
    if (!ShaderCompiler::Initialize()) {
        std::cerr << "[Engine] Falha ao inicializar DXC shader compiler\n";
        return false;
    }
    m_shaderCompilerInitialized = true;

    if (!m_renderer.Initialize(
            m_device.GetDevice(),
            m_window.GetWidth(),
            m_window.GetHeight(),
            L"Shaders/Basic.hlsl",
            L"Shaders/DeferredLighting.hlsl")) {
        std::cerr << "[Engine] Falha ao inicializar Renderer\n";
        return false;
    }

    m_cpuProfiler.Init();

    if (!m_gpuProfiler.Initialize(m_device.GetDevice(), m_device.GetDirectQueue())) {
        std::cerr << "[Engine] GPU profiler initialization failed (non-fatal)\n";
    }

    return true;
}

bool Application::InitializeTerrain() {
    m_jobSystem = std::make_unique<JobSystem>();

    if (!m_copyEngine.Initialize(m_device.GetDevice())) {
        std::cerr << "[Engine] Falha ao inicializar CopyEngine\n";
        return false;
    }
    m_copyEngineInitialized = true;

    m_game = std::make_unique<InteractiveTerrainGame>();

    const GameInitializeContext initContext{
        m_window,
        m_device.GetDevice(),
        m_device.GetDirectQueue(),
        m_copyEngine,
        *m_jobSystem
    };
    if (!m_game->Initialize(initContext)) {
        m_game.reset();
        return false;
    }

    return true;
}

bool Application::InitializeEditor() {
    if (!m_imguiLayer.Initialize(m_window, m_device)) {
        std::cerr << "[Engine] Falha ao inicializar ImGuiLayer\n";
        return false;
    }
    m_imguiInitialized = true;
    return true;
}

void Application::InitializeTiming() {
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastTime);
}

void Application::BeginFrame() {
    m_cpuProfiler.FrameStart();
    m_window.PollEvents();

    m_game->HandleInput(m_window);
    m_renderer.HandleResizeIfNeeded(m_context, m_device, m_window, m_game->GetCamera(), m_imguiLayer);

    UpdateDeltaTime();
}

bool Application::UpdateEditor() {
    const GameUpdateContext updateContext{
        m_window,
        m_deltaTime,
        m_imguiLayer.WantsCaptureMouse()
    };
    m_game->Update(updateContext);

    m_imguiLayer.BeginFrame();

    const GameUiContext uiContext{
        m_context,
        m_device,
        m_cpuProfiler,
        m_gpuProfiler,
        m_deltaTime
    };
    if (!m_game->DrawUi(uiContext)) {
        m_exitCode = -1;
        return false;
    }

    return true;
}

void Application::UpdateTerrain() {
    m_cpuProfiler.BeginTerrainUpdate();
    const uint32_t frameIndex = static_cast<uint32_t>(m_device.GetFrameIndex());
    m_game->UpdateSimulation(frameIndex);
    m_cpuProfiler.EndTerrainUpdate();
}

void Application::RenderFrame() {
    RenderFrameCallbacks callbacks;
    callbacks.recordUploads = [this](ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex) {
        m_game->RecordUploads(cmdList, frameIndex);
    };
    callbacks.drawScene = [this](ID3D12GraphicsCommandList* cmdList, const Frustum& frustum) {
        m_game->DrawScene(cmdList, frustum);
    };

    m_renderer.RenderFrame(
        m_context,
        m_device,
        m_game->GetCamera(),
        m_cpuProfiler,
        m_gpuProfiler,
        callbacks);
}

void Application::WaitForGpuIfNeeded() {
    if (m_contextInitialized && m_deviceInitialized) {
        m_context.WaitForGPU(m_device.GetDirectQueue());
    }
}

void Application::ShutdownEditor() {
    if (m_imguiInitialized) {
        m_imguiLayer.Shutdown();
        m_imguiInitialized = false;
    }
}

void Application::ShutdownRendering() {
    if (m_contextInitialized) {
        m_context.Shutdown();
        m_contextInitialized = false;
    }

    if (m_shaderCompilerInitialized) {
        ShaderCompiler::Shutdown();
        m_shaderCompilerInitialized = false;
    }
}

void Application::ShutdownTerrain() {
    m_game.reset();

    if (m_copyEngineInitialized) {
        m_copyEngine.Shutdown();
        m_copyEngineInitialized = false;
    }

    m_jobSystem.reset();
}

void Application::ShutdownPlatform() {
    if (m_deviceInitialized) {
        m_device.Shutdown();
        m_deviceInitialized = false;
    }

    if (m_windowInitialized) {
        m_window.Shutdown();
        m_windowInitialized = false;
    }
}
