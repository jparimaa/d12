#pragma once

#include "Camera.h"

#include <DirectXMath.h>

namespace fw
{
class CameraController
{
public:
    CameraController(){};
    CameraController(const CameraController&) = delete;
    CameraController(CameraController&&) = delete;
    CameraController& operator=(const CameraController&) = delete;
    CameraController& operator=(CameraController&&) = delete;

    void setCamera(Camera* camera);
    void setMovementSpeed(float speed);
    void setSensitivity(float sensitivity);
    void setResetMode(float px, float py, float pz, float rx, float ry, float rz, int key);
    void update();

private:
    Camera* m_camera = nullptr;
    float m_movementSpeed = 5.0f;
    float m_sensitivity = 0.003f;
    DirectX::XMVECTOR m_resetPosition;
    DirectX::XMVECTOR m_resetRotation;
    int m_resetKey = -1;
    bool m_rotationEnabled = true;
};

} // namespace fw
