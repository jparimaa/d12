#pragma once

#include <DirectXMath.h>

namespace fw
{
class Transformation
{
public:
    static const DirectX::XMVECTOR c_forward;
    static const DirectX::XMVECTOR c_up;
    static const DirectX::XMVECTOR c_left;

    DirectX::XMVECTOR position;
    DirectX::XMVECTOR rotation;
    DirectX::XMVECTOR scale;

    Transformation();
    ~Transformation();

    void setPosition(float x, float y, float z);
    void move(const DirectX::XMFLOAT3& translation);
    void move(const DirectX::XMVECTOR& translation);
    void rotate(const DirectX::XMFLOAT3& axis, float amount);
    void rotate(const DirectX::XMVECTOR& axis, float amount);

    DirectX::XMVECTOR getForward() const;
    DirectX::XMVECTOR getUp() const;
    DirectX::XMVECTOR getLeft() const;

    const DirectX::XMMATRIX& updateWorldMatrix();
    const DirectX::XMMATRIX& getWorldMatrix() const;

private:
    DirectX::XMVECTOR m_forward;
    DirectX::XMVECTOR m_up;
    DirectX::XMVECTOR m_left;
    DirectX::XMMATRIX m_worldMatrix;
};

} // namespace fw
