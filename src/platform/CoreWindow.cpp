#include "CoreWindow.h"

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    CoreWindow* coreWindow = static_cast<CoreWindow*>(glfwGetWindowUserPointer(window));
    if (coreWindow) {
        coreWindow->UpdateDimensions(width, height);
    }
}

bool CoreWindow::Initialize(int width, int height, const char* title) {
    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // DirectX doesn't need an OpenGL context
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }

    m_hwnd = glfwGetWin32Window(m_window);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth > 0 && framebufferHeight > 0) {
        m_width = framebufferWidth;
        m_height = framebufferHeight;
    } else {
        m_width = width;
        m_height = height;
    }

    return true;
}

void CoreWindow::Shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void CoreWindow::PollEvents() {
    glfwPollEvents();
}

bool CoreWindow::ShouldClose() const {
    return glfwWindowShouldClose(m_window);
}
