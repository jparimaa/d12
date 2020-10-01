RWTexture2D<float4> tex : register(u0);

[numthreads(8, 8, 1)]
void main( 
    uint3 groupID : SV_GroupID, 
    uint groupIndex : SV_GroupIndex, 
    uint3 groupThreadID : SV_GroupThreadID, 
    uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 sum = 0.0;
    sum += tex[dispatchThreadID.xy + uint2(-1, -1)];
    sum += tex[dispatchThreadID.xy + uint2(0, -1)];
    sum += tex[dispatchThreadID.xy + uint2(1, -1)];
    sum += tex[dispatchThreadID.xy + uint2(-1, 0)];
    sum += tex[dispatchThreadID.xy + uint2(0, 0)];
    sum += tex[dispatchThreadID.xy + uint2(1, 0)];
    sum += tex[dispatchThreadID.xy + uint2(-1, 1)];
    sum += tex[dispatchThreadID.xy + uint2(0, 1)];
    sum += tex[dispatchThreadID.xy + uint2(1, 1)];
    sum /= 9.0;    
    DeviceMemoryBarrier();
    tex[dispatchThreadID.xy] = sum;
}
