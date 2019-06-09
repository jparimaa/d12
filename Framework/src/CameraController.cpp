#include "CameraController.h"
#include "API.h"
#include "Transformation.h"

#include <GLFW/glfw3.h>

#include <cassert>

using namespace DirectX;

namespace
{
const float c_rotationLimit = DirectX::XMConvertToRadians(87.0f);
}

namespace fw
{
void CameraController::setCamera(Camera* camera)
{
    m_camera = camera;
}

void CameraController::setMovementSpeed(float speed)
{
    m_movementSpeed = speed;
}

void CameraController::setSensitivity(float sensitivity)
{
    m_sensitivity = sensitivity;
}

void CameraController::setResetMode(float px, float py, float pz, float rx, float ry, float rz, int key)
{
    m_resetPosition = DirectX::XMVectorSet(px, py, pz, 0.0f);
    m_resetRotation = DirectX::XMVectorSet(rx, ry, rz, 0.0f);
    m_resetKey = key;
}

void CameraController::update()
{
    assert(m_camera != nullptr);

    float speed = m_movementSpeed * API::getTimeDelta();
    Transformation& t = m_camera->getTransformation();

    if (API::isKeyDown(GLFW_KEY_W))
    {
        t.move(t.getForward() * speed);
    }
    if (API::isKeyDown(GLFW_KEY_S))
    {
        t.move(-t.getForward() * speed);
    }
    if (API::isKeyDown(GLFW_KEY_A))
    {
        t.move(t.getLeft() * speed);
    }
    if (API::isKeyDown(GLFW_KEY_D))
    {
        t.move(-t.getLeft() * speed);
    }
    if (API::isKeyReleased(m_resetKey))
    {
        t.setPosition(m_resetPosition);
        t.setRotation(m_resetRotation);
    }
    if (API::isKeyReleased(GLFW_KEY_F))
    {
        m_rotationEnabled = !m_rotationEnabled;
    }

    if (m_rotationEnabled)
    {
        t.rotate(Transformation::c_up, -API::getMouseDeltaX() * m_sensitivity);
        t.rotate(Transformation::c_left, API::getMouseDeltaY() * m_sensitivity);

        XMFLOAT3 r;
        XMStoreFloat3(&r, t.rotation);

        if (r.x > c_rotationLimit)
        {
            r.x = c_rotationLimit;
        }
        if (r.x < -c_rotationLimit)
        {
            r.x = -c_rotationLimit;
        }
        t.setRotation(r);
    }
}

} // namespace fw
