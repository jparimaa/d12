#pragma once

#include <DirectXMath.h>

namespace fw
{
class Transformation
{
public:
    static const DirectX::XMVECTOR FORWARD;
    static const DirectX::XMVECTOR UP;
    static const DirectX::XMVECTOR LEFT;

    DirectX::XMVECTOR position;
    DirectX::XMVECTOR rotation;
    DirectX::XMVECTOR scale;

    Transformation();
    ~Transformation();

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
    DirectX::XMVECTOR forward;
    DirectX::XMVECTOR up;
    DirectX::XMVECTOR left;
    DirectX::XMMATRIX worldMatrix;
};

} // namespace fw
