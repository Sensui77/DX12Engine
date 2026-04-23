#include "ImGuiLayer.h"

#include "CoreWindow.h"
#include "DX12Device.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_glfw.h"

bool ImGuiLayer::Initialize(const CoreWindow& window, DX12Device& device) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_contextInitialized = true;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOther(window.GetGlfwWindow(), true)) {
        Shutdown();
        return false;
    }
    m_glfwBackendInitialized = true;

    if (!CreateDeviceObjects(window, device)) {
        Shutdown();
        return false;
    }

    return true;
}

void ImGuiLayer::Shutdown() {
    InvalidateDeviceObjects();

    if (m_glfwBackendInitialized) {
        ImGui_ImplGlfw_Shutdown();
        m_glfwBackendInitialized = false;
    }

    if (m_contextInitialized) {
        ImGui::DestroyContext();
        m_contextInitialized = false;
    }
}

bool ImGuiLayer::CreateDeviceObjects(const CoreWindow& window, DX12Device& device) {
    if (!ImGui_ImplDX12_Init(device.GetDevice(), MAX_FRAMES_IN_FLIGHT,
            DXGI_FORMAT_R8G8B8A8_UNORM, device.GetUiSrvHeap(),
            device.GetUiSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
            device.GetUiSrvHeap()->GetGPUDescriptorHandleForHeapStart())) {
        return false;
    }

    m_dx12BackendInitialized = true;
    SyncDisplaySize(window);
    return true;
}

void ImGuiLayer::InvalidateDeviceObjects() {
    if (m_dx12BackendInitialized) {
        ImGui_ImplDX12_Shutdown();
        m_dx12BackendInitialized = false;
    }
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

bool ImGuiLayer::WantsCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void ImGuiLayer::SyncDisplaySize(const CoreWindow& window) {
    ImGuiIO& io = ImGui::GetIO();

    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window.GetGlfwWindow(), &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window.GetGlfwWindow(), &framebufferWidth, &framebufferHeight);

    io.DisplaySize = ImVec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    if (windowWidth > 0 && windowHeight > 0) {
        io.DisplayFramebufferScale = ImVec2(
            static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth),
            static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight));
    }
}
