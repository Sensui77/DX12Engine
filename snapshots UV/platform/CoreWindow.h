#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class CoreWindow {
public:
    CoreWindow() = default;
    ~CoreWindow() = default;

    bool Initialize(int width, int height, const char* title);
    void Shutdown();
    void PollEvents();
    bool ShouldClose() const;
    
    bool WasResized() const { return m_resized; }
    void ResetResizeFlag() { m_resized = false; }
    void UpdateDimensions(int w, int h) { m_width = w; m_height = h; m_resized = true; }
    
    // Getters
    HWND GetHwnd() const { return m_hwnd; }
    GLFWwindow* GetGlfwWindow() const { return m_window; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    GLFWwindow* m_window = nullptr;
    HWND m_hwnd = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_resized = false;
};
