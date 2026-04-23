#pragma once
#include <DirectXMath.h>
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera();
    ~Camera() = default;

    void Initialize(float fovDegrees, float aspectRatio, float nearZ, float farZ);
    void Update(GLFWwindow* window, float deltaTime);
    void SetActive(bool active); // Habilita/desabilita processamento de input
    void SetAspectRatio(float aspectRatio);

    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    DirectX::XMMATRIX GetViewProjectionMatrix() const;

private:
    void UpdateVectors();

    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_forward;
    DirectX::XMFLOAT3 m_up;
    DirectX::XMFLOAT3 m_right;

    float m_yaw;
    float m_pitch;

    float m_movementSpeed;
    float m_mouseSensitivity;

    float m_fov;
    float m_aspectRatio;
    float m_nearZ;
    float m_farZ;

    bool m_firstMouse;
    double m_lastMouseX;
    double m_lastMouseY;
    bool m_active = true; // false quando cursor livre (menu)
};
