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

struct PerFrame
{
    float4 A;
    float4 B;
    float4 C;
};

StructuredBuffer<Index> indexBuffer : register(t0);
StructuredBuffer<Vertex> vertexBuffer : register(t1);
StructuredBuffer<PerFrame> perFrame : register(t2);

[shader("closesthit")] void ClosestHit(inout HitInfo payload,
                                       Attributes attributes) {
    float3 barycentrics = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics.x, attributes.barycentrics.y);
    float3 hitColor = perFrame[0].A * barycentrics.x + perFrame[0].B * barycentrics.y + perFrame[0].C * barycentrics.z;
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
