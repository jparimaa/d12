struct VertexIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer cbPerObject : register(b0)
{
    uint index;
};

struct Matrix
{
    float4x4 wvp;
};

StructuredBuffer<Matrix> worldViewProjs : register(t0);
Texture2D textures[] : register(t1);
SamplerState pointSampler : register(s0);

VertexOut VS(VertexIn vertexIn)
{
    VertexOut vertexOut;
    vertexOut.position = mul(float4(vertexIn.position, 1.0f), worldViewProjs[index].wvp);
    vertexOut.uv = vertexIn.uv;
    return vertexOut;
}

float4 PS(VertexOut vertexOut) :
    SV_Target
{
    return textures[index].Sample(pointSampler, vertexOut.uv);
}
