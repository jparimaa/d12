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

[shader("closesthit")] void ClosestHit(inout HitInfo payload,
                                       Attributes attributes) {
    float3 barycentrics = float3(1.0 - attributes.barycentrics.x - attributes.barycentrics.y, attributes.barycentrics.x, attributes.barycentrics.y);
    int index = PrimitiveIndex() * 3;
    float3 hitColor = //
        vertexBuffer[indexBuffer[index + 0].index].normal * barycentrics.x + //
        vertexBuffer[indexBuffer[index + 1].index].normal * barycentrics.y + //
        vertexBuffer[indexBuffer[index + 2].index].normal * barycentrics.z;
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
