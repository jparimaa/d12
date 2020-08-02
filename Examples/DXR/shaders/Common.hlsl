struct HitInfo
{
    float4 colorAndDistance;
};

struct Attributes
{
    float2 barycentrics;
};

float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}