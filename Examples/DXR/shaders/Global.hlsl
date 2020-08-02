cbuffer CameraParams : register(b0)
{
    float4x4 inverseView;
    float4x4 inverseProjection;
}
RaytracingAccelerationStructure tlas : register(t0);