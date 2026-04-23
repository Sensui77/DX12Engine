#include "Camera.h"
#include <algorithm>

using namespace DirectX;

Camera::Camera() 
    : m_position(0.0f, 40.0f, -20.0f),
      m_forward(0.0f, 0.0f, 1.0f),
      m_up(0.0f, 1.0f, 0.0f),
      m_right(1.0f, 0.0f, 0.0f),
      m_yaw(90.0f),
      m_pitch(-45.0f),
      m_movementSpeed(50.0f),
      m_mouseSensitivity(0.1f),
      m_fov(90.0f),
      m_aspectRatio(16.0f / 9.0f),
      m_nearZ(0.1f),
      m_farZ(1000.0f),
      m_firstMouse(true),
      m_lastMouseX(0.0),
      m_lastMouseY(0.0) 
{
    UpdateVectors();
}

void Camera::Initialize(float fovDegrees, float aspectRatio, float nearZ, float farZ) {
    m_fov = fovDegrees;
    m_aspectRatio = aspectRatio;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

void Camera::SetActive(bool active) {
    m_active = active;
    if (active) {
        // Forca re-inicializacao do mouse para evitar salto ao reativar
        m_firstMouse = true;
    }
}

void Camera::SetAspectRatio(float aspectRatio) {
    m_aspectRatio = aspectRatio;
}

void Camera::Update(GLFWwindow* window, float deltaTime) {
    if (!m_active) return; // Sem input quando cursor livre

    // 1. Keyboard Input for Movement
    float velocity = m_movementSpeed * deltaTime;
    
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    XMVECTOR right = XMLoadFloat3(&m_right);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        pos = XMVectorAdd(pos, XMVectorScale(forward, velocity));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        pos = XMVectorSubtract(pos, XMVectorScale(forward, velocity));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        pos = XMVectorSubtract(pos, XMVectorScale(right, velocity));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        pos = XMVectorAdd(pos, XMVectorScale(right, velocity));

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        pos = XMVectorAdd(pos, XMVectorScale(XMLoadFloat3(&m_up), velocity));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        pos = XMVectorSubtract(pos, XMVectorScale(XMLoadFloat3(&m_up), velocity));

    XMStoreFloat3(&m_position, pos);

    // 2. Mouse Input for Look Around
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (m_firstMouse) {
        m_lastMouseX = xpos;
        m_lastMouseY = ypos;
        m_firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - m_lastMouseX);
    float yoffset = static_cast<float>(m_lastMouseY - ypos); // Reversed since y-coordinates range from bottom to top in math

    m_lastMouseX = xpos;
    m_lastMouseY = ypos;

    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    // Invertido: Subtrair o deslocamento resolve a matemática da câmera no HLSL LH
    m_yaw -= xoffset;

    m_pitch += yoffset;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    UpdateVectors();
}

void Camera::UpdateVectors() {
    // Calcula novo vetor direcional basedo em Yaw e Pitch (Spherical coordinates)
    XMFLOAT3 front;
    front.x = cos(XMConvertToRadians(m_yaw)) * cos(XMConvertToRadians(m_pitch));
    front.y = sin(XMConvertToRadians(m_pitch));
    front.z = sin(XMConvertToRadians(m_yaw)) * cos(XMConvertToRadians(m_pitch));

    XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&front));
    XMStoreFloat3(&m_forward, forward);

    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    XMStoreFloat3(&m_right, right);

    XMVECTOR up = XMVector3Cross(forward, right);
    XMStoreFloat3(&m_up, up);
}

XMMATRIX Camera::GetViewMatrix() const {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    XMVECTOR up = XMLoadFloat3(&m_up);

    return XMMatrixLookAtLH(pos, XMVectorAdd(pos, forward), up);
}

XMMATRIX Camera::GetProjectionMatrix() const {
    return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov), m_aspectRatio, m_nearZ, m_farZ);
}

XMMATRIX Camera::GetViewProjectionMatrix() const {
    return XMMatrixMultiply(GetViewMatrix(), GetProjectionMatrix());
}
