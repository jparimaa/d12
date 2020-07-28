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

StructuredBuffer<Index> indexBuffer : register(t0);
StructuredBuffer<Vertex> vertexBuffer : register(t1);

[shader("closesthit")] void ClosestHit(inout HitInfo payload,
                                       Attributes attrib) {
    uint primitiveId = 3 * PrimitiveIndex();
    float2 n = vertexBuffer[indexBuffer[primitiveId].index].uv;
    float3 hitColor;
    hitColor.x = n.x;
    hitColor.y = n.y;
    hitColor.z = 0.3;
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
