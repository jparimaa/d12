#include "Common.hlsl"

struct Index
{
    int index;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

// For debugging purposes
struct PerFrame
{
    float4 A;
    float4 B;
    float4 C;
};

// Could use ByteAddressBuffer & 16bit indices for smaller index buffer
StructuredBuffer<Index> indexBuffer : register(t0);
StructuredBuffer<Vertex> vertexBuffer : register(t1);
StructuredBuffer<PerFrame> perFrame : register(t2);

float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

[shader("closesthit")] void ClosestHit(inout HitInfo payload,
                                       Attributes attributes) {
    const float3 barycentrics = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics.x, attributes.barycentrics.y);
    const int index = PrimitiveIndex() * 3;
    const float3 normal = //
        vertexBuffer[indexBuffer[index + 0].index].normal * barycentrics.x + //
        vertexBuffer[indexBuffer[index + 1].index].normal * barycentrics.y + //
        vertexBuffer[indexBuffer[index + 2].index].normal * barycentrics.z;

    const float3 hitPosition = HitWorldPosition();
    const float3 lightPosition = float3(10.0, 20.0, -10.0);
    const float3 pixelToLight = normalize(lightPosition - hitPosition);
    const float NDotL = max(0.0f, dot(pixelToLight, normal));
    const float3 lightFactor = NDotL;

    const float3 debugColor = perFrame[0].A * barycentrics.x + perFrame[0].B * barycentrics.y + perFrame[0].C * barycentrics.z;
    const float3 hitColor = lerp(lightFactor, debugColor, 0.3);
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
