cbuffer cbPerObject : register(b0)
{
	float4x4 worldViewProj; 
};

struct VertexIn
{
	float3 position : POSITION;
    float3 normal: NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD;
};

struct VertexOut
{
	float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

VertexOut VS(VertexIn vertexIn)
{
	VertexOut vertexOut;
	vertexOut.position = mul(float4(vertexIn.position, 1.0f), worldViewProj);
    vertexOut.uv = vertexIn.uv;    
    return vertexOut;
}

float4 PS(VertexOut vertexOut) : SV_Target
{
    return g_texture.Sample(g_sampler, vertexOut.uv);
}


