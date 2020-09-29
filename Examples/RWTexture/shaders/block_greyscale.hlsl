RWTexture2D<float> tex : register(u0);

[numthreads(8, 8, 1)]
void main( 
    uint3 groupID : SV_GroupID, 
    uint groupIndex : SV_GroupIndex, 
    uint3 groupThreadID : SV_GroupThreadID, 
    uint3 dispatchThreadID : SV_DispatchThreadID)
{
    tex[dispatchThreadID.xy] *= float3(0.3, 0.59, 0.11);
}