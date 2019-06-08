#include "Transformation.h"

using namespace DirectX;

namespace fw
{
const XMVECTOR Transformation::FORWARD = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
const XMVECTOR Transformation::UP = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
const XMVECTOR Transformation::LEFT = XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f);

Transformation::Transformation() :
    position(XMVectorZero()),
    rotation(XMVectorZero()),
    scale(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f))
{
}

Transformation::~Transformation()
{
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
    return XMVector4Transform(FORWARD, XMMatrixRotationRollPitchYawFromVector(rotation));
}

XMVECTOR Transformation::getUp() const
{
    return XMVector4Transform(UP, XMMatrixRotationRollPitchYawFromVector(rotation));
}

XMVECTOR Transformation::getLeft() const
{
    return XMVector4Transform(LEFT, XMMatrixRotationRollPitchYawFromVector(rotation));
}

const XMMATRIX& Transformation::updateWorldMatrix()
{
    worldMatrix = XMMatrixScalingFromVector(scale) * XMMatrixRotationRollPitchYawFromVector(rotation) * XMMatrixTranslationFromVector(position);
    return worldMatrix;
}

const XMMATRIX& Transformation::getWorldMatrix() const
{
    return worldMatrix;
}

} // namespace fw