#pragma once

class CoreWindow;
class DX12Device;

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    bool Initialize(const CoreWindow& window, DX12Device& device);
    void Shutdown();

    bool CreateDeviceObjects(const CoreWindow& window, DX12Device& device);
    void InvalidateDeviceObjects();
    void BeginFrame();

    bool WantsCaptureMouse() const;

private:
    void SyncDisplaySize(const CoreWindow& window);

    bool m_contextInitialized = false;
    bool m_glfwBackendInitialized = false;
    bool m_dx12BackendInitialized = false;
};
