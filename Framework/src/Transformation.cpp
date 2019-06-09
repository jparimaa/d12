#include "Transformation.h"

using namespace DirectX;

namespace fw
{
const XMVECTOR Transformation::c_forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
const XMVECTOR Transformation::c_up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
const XMVECTOR Transformation::c_left = XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);

Transformation::Transformation() :
    position(XMVectorZero()),
    rotation(XMVectorZero()),
    scale(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f))
{
}

Transformation::~Transformation()
{
}

void Transformation::setPosition(float x, float y, float z)
{
    position = DirectX::XMVectorSet(x, y, z, 0.0f);
}

void Transformation::setPosition(const DirectX::XMVECTOR& p)
{
    position = p;
}

void Transformation::setRotation(const DirectX::XMVECTOR& r)
{
    rotation = r;
}

void Transformation::setRotation(const DirectX::XMFLOAT3& r)
{
    rotation = XMLoadFloat3(&r);
}

void Transformation::move(const XMFLOAT3& translation)
{
    position += XMLoadFloat3(&translation);
}

void Transformation::move(const XMVECTOR& translation)
{
    position += translation;
}

void Transformation::rotate(const XMFLOAT3& axis, float amount)
{
    rotation += XMLoadFloat3(&axis) * amount;
}

void Transformation::rotate(const XMVECTOR& axis, float amount)
{
    rotation += axis * amount;
}

XMVECTOR Transformation::getForward() const
{
    return XMVector4Transform(c_forward, XMMatrixRotationRollPitchYawFromVector(rotation));
}

XMVECTOR Transformation::getUp() const
{
    return XMVector4Transform(c_up, XMMatrixRotationRollPitchYawFromVector(rotation));
}

XMVECTOR Transformation::getLeft() const
{
    return XMVector4Transform(c_left, XMMatrixRotationRollPitchYawFromVector(rotation));
}

const XMMATRIX& Transformation::updateWorldMatrix()
{
    m_worldMatrix = XMMatrixScalingFromVector(scale) * XMMatrixRotationRollPitchYawFromVector(rotation) * XMMatrixTranslationFromVector(position);
    return m_worldMatrix;
}

const XMMATRIX& Transformation::getWorldMatrix() const
{
    return m_worldMatrix;
}

} // namespace fw