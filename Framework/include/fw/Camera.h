#pragma once

#include "Transformation.h"

#include <DirectXMath.h>

namespace fw
{
class Camera
{
public:
    Camera();

    Transformation& getTransformation();

    void updateViewMatrix();
    void updateProjectionMatrix();

    const DirectX::XMMATRIX& getViewMatrix() const;
    const DirectX::XMMATRIX& getProjectionMatrix() const;

    void setNearClipDistance(float distance);
    void setFarClipDistance(float distance);
    float getNearClipDistance() const;
    float getFarClipDistance() const;

private:
    float m_FOV = 0.785f;
    float m_ratio = 1.33333f;
    float m_nearClipDistance = 0.1f;
    float m_farClipDistance = 100.0f;

    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projectionMatrix;

    Transformation m_transformation;
};

} // namespace fw
