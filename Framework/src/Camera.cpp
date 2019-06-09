#include "Camera.h"

#include "API.h"

using namespace DirectX;

namespace fw
{
Camera::Camera()
{
    updateViewMatrix();
    updateProjectionMatrix();
}

Transformation& Camera::getTransformation()
{
    return m_transformation;
}

void Camera::updateViewMatrix()
{
    m_viewMatrix = XMMatrixLookAtLH(m_transformation.position,
                                    m_transformation.position + m_transformation.getForward(),
                                    m_transformation.getUp());
}

void Camera::updateProjectionMatrix()
{
    m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(m_FOV, fw::API::getAspectRatio(), m_nearClipDistance, m_farClipDistance);
}

const XMMATRIX& Camera::getViewMatrix() const
{
    return m_viewMatrix;
}

const XMMATRIX& Camera::getProjectionMatrix() const
{
    return m_projectionMatrix;
}

void Camera::setNearClipDistance(float distance)
{
    m_nearClipDistance = distance;
    updateProjectionMatrix();
}

void Camera::setFarClipDistance(float distance)
{
    m_farClipDistance = distance;
    updateProjectionMatrix();
}

float Camera::getNearClipDistance() const
{
    return m_nearClipDistance;
}

float Camera::getFarClipDistance() const
{
    return m_farClipDistance;
}
} // namespace fw
