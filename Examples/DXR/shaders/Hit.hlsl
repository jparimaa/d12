#include "Common.hlsl"

struct Vertex
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
};

StructuredBuffer<Vertex> BTriVertex : register(t0);

[shader("closesthit")] void ClosestHit(inout HitInfo payload,
                                       Attributes attrib) {
    uint vertId = 3 * PrimitiveIndex();
    float3 hitColor;
    hitColor.x = attrib.bary.x;
    hitColor.y = attrib.bary.y;
    hitColor.z = 1.0;
    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}
